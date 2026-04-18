        // Runtime pipeline: lifecycle, workers, queuing, networking, actors, and sampling.
        inline void shutdown() noexcept {
            // EventLoop dtor can walk peers and invoke callbacks through cb_live.
            // In main(), UdpCallbacks is a stack object with shorter lifetime than app,
            // so clear callback hook before app members start tearing down.
            loop.cb_live = nullptr;
            region_shutdown(io::monotonic_ms());

            if (pool) pool->shutdown(true);
            if (worker_slots) {
                for (io::u32 i = 0; i < worker.slot_count; ++i) worker_slots[i].~WorkerSlot();
            }
            worker_slots = nullptr;
            worker_slots_mem.reset(nullptr);
            worker_args = nullptr;
            worker_args_mem.reset(nullptr);
            if (pool) pool->~ThreadPool();
            pool = nullptr;
            pool_mem.reset(nullptr);

            if (terrain) terrain->~SimplexTerrain();
            terrain = nullptr;
            terrain_mem.reset(nullptr);
            world.clear();

            pending = nullptr;
            pending_mem.reset(nullptr);
            peers = nullptr;
            peers_mem.reset(nullptr);
            stream_order = nullptr;
            stream_order_mem.reset(nullptr);
            stream_state = nullptr;
            stream_state_mem.reset(nullptr);
            stream_reqid = nullptr;
            stream_reqid_mem.reset(nullptr);
            stream_sent_ms = nullptr;
            stream_sent_ms_mem.reset(nullptr);
            sand_touched = nullptr;
            sand_touched_mem.reset(nullptr);
            sand_reserved = nullptr;
            sand_reserved_mem.reset(nullptr);
            sand_active = nullptr;
            sand_active_mem.reset(nullptr);
            water_touched = nullptr;
            water_touched_mem.reset(nullptr);
            water_edits = nullptr;
            water_edits_mem.reset(nullptr);
            water_active = nullptr;
            water_active_mem.reset(nullptr);
            deferred_block_edits = nullptr;
            deferred_block_edits_mem.reset(nullptr);
            deferred_block_edit_head = 0u;
            deferred_block_edit_tail = 0u;
            deferred_block_edit_count = 0u;
            hot_sim = nullptr;
            hot_sim_mem.reset(nullptr);
            hot_sim_cap = 0u;
            hot_sim_last_idx = 0u;
            if (world_actor_ecs) world_actor_ecs->~ActorEcs();
            world_actor_ecs = nullptr;
            world_actor_ecs_mem.reset(nullptr);
            if (player_ecs) player_ecs->~PlayerEcs();
            player_ecs = nullptr;
            player_ecs_mem.reset(nullptr);
            part_buf = nullptr;
            part_buf_mem.reset(nullptr);
            recv_buf = nullptr;
            recv_buf_mem.reset(nullptr);
        }

        static void worker_entry(void* arg) noexcept {
            WorkerArg* wa = reinterpret_cast<WorkerArg*>(arg);
            if (!wa || !wa->self) return;
            wa->self->run_worker(wa->slot);
        }

        inline void run_worker(io::u32 slot_index) noexcept {
            if (!terrain || !worker_slots || slot_index >= worker.slot_count) return;
            WorkerSlot& slot = worker_slots[slot_index];
            if (slot.state.load(io::memory_order_acquire) != SlotQueued) return;
            slot.state.store(SlotRunning, io::memory_order_release);
            if (slot.cancel.load(io::memory_order_acquire) != 0u) {
                slot.state.store(SlotReady, io::memory_order_release);
                return;
            }
            slot.chunk.coord = { slot.job.req.cx, slot.job.req.cy, slot.job.req.cz };
            if (!ge::voxel::load_chunk_binary(slot.chunk.coord, slot.chunk)) {
                terrain_lock.lock();
                terrain->generate_chunk(slot.chunk);
                terrain_lock.unlock();
            }
            slot.chunk.generated = true;
            slot.chunk.dirty_mesh = false;
            slot.chunk.dirty_neighbors = false;
            if (!build_chunk_wire_payload(slot))
                slot.cancel.store(1u, io::memory_order_release);
            slot.state.store(SlotReady, io::memory_order_release);
        }

        IO_NODISCARD inline io::u32 find_slot(io::u32 wanted_state, io::u32& cursor) noexcept {
            if (!worker_slots || worker.slot_count == 0u) return INVALID_SLOT;
            if (cursor >= worker.slot_count) cursor = 0u;
            for (io::u32 i = 0; i < worker.slot_count; ++i) {
                const io::u32 idx = (cursor + i) % worker.slot_count;
                if (worker_slots[idx].state.load(io::memory_order_acquire) == wanted_state) {
                    cursor = (idx + 1u) % worker.slot_count;
                    return idx;
                }
            }
            return INVALID_SLOT;
        }

        IO_NODISCARD inline bool is_duplicate(io::Endpoint to, const ge::net::ChunkRequest& req) const noexcept {
            for (io::u32 i = 0; i < pending_count; ++i) {
                const io::u32 idx = (pending_head + i) % MAX_PENDING;
                const PendingReq& p = pending[idx];
                if (!endpoint_eq(p.to, to)) continue;
                if (p.req.cx == req.cx && p.req.cy == req.cy && p.req.cz == req.cz && p.req.lod == req.lod) return true;
            }
            if (!worker_slots) return false;
            for (io::u32 i = 0; i < worker.slot_count; ++i) {
                const WorkerSlot& s = worker_slots[i];
                const io::u32 st = s.state.load(io::memory_order_acquire);
                if (st == SlotFree) continue;
                if (s.cancel.load(io::memory_order_acquire) != 0u) continue;
                if (!endpoint_eq(s.job.to, to)) continue;
                if (s.job.req.cx == req.cx && s.job.req.cy == req.cy && s.job.req.cz == req.cz && s.job.req.lod == req.lod) return true;
            }
            return false;
        }

        IO_NODISCARD inline io::u32 request_priority(io::u16 peer_index, const ge::net::ChunkRequest& req) const noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used) {
                const io::i32 ax = req.cx < 0 ? -req.cx : req.cx;
                const io::i32 ay = req.cy < 0 ? -req.cy : req.cy;
                const io::i32 az = req.cz < 0 ? -req.cz : req.cz;
                return static_cast<io::u32>(ax + ay + az);
            }

            const PeerState& p = peers[peer_index];
            float px = 0.f, py = 0.f, pz = 0.f;
            if (p.has_pending) {
                px = p.pending_x;
                py = p.pending_y;
                pz = p.pending_z;
            } else if (p.has_auth) {
                px = p.auth_x;
                py = p.auth_y;
                pz = p.auth_z;
            }

            ge::voxel::ChunkCoord center{};
            io::u32 lx = 0, ly = 0, lz = 0;
            ge::voxel::split_world_coord(floor_to_i32(px), floor_to_i32(py), floor_to_i32(pz), center, lx, ly, lz);
            const io::i32 dx = req.cx - center.x;
            const io::i32 dy = req.cy - center.y;
            const io::i32 dz = req.cz - center.z;
            const io::u32 dist2 = static_cast<io::u32>(dx * dx + dy * dy + dz * dz);
            io::u32 priority = dist2 * 1024u;

            if (dy < 0) {
                const io::u32 bonus = static_cast<io::u32>(-dy) * 96u;
                priority = (priority > bonus) ? (priority - bonus) : 0u;
            } else {
                priority += static_cast<io::u32>(dy) * 96u;
            }

            if (p.has_auth) {
                io::i32 dir_x = p.move_dir_x;
                io::i32 dir_y = p.move_dir_y;
                io::i32 dir_z = p.move_dir_z;
                if (p.has_pending) {
                if (p.pending_x > p.auth_x + 0.01f) dir_x = 1;
                else if (p.pending_x < p.auth_x - 0.01f) dir_x = -1;
                if (p.pending_y > p.auth_y + 0.01f) dir_y = 1;
                else if (p.pending_y < p.auth_y - 0.01f) dir_y = -1;
                if (p.pending_z > p.auth_z + 0.01f) dir_z = 1;
                else if (p.pending_z < p.auth_z - 0.01f) dir_z = -1;
                }

                const io::i32 ahead = dx * dir_x + dy * dir_y + dz * dir_z;
                if (ahead > 0) {
                    const io::u32 bonus = static_cast<io::u32>(ahead) * 256u;
                    priority = (priority > bonus) ? (priority - bonus) : 0u;
                }
            }
            return priority;
        }

        IO_NODISCARD inline bool dequeue_best_req(PendingReq& out_req) noexcept {
            if (!pending || pending_count == 0u) return false;

            io::u32 best_rel = 0u;
            io::u32 best_idx = pending_head;
            io::u32 best_priority = pending[best_idx].priority;
            for (io::u32 rel = 1u; rel < pending_count; ++rel) {
                const io::u32 idx = (pending_head + rel) % MAX_PENDING;
                const io::u32 pri = pending[idx].priority;
                if (pri < best_priority) {
                    best_priority = pri;
                    best_rel = rel;
                    best_idx = idx;
                }
            }

            out_req = pending[best_idx];

            io::u32 dst = 0u;
            for (io::u32 rel = 0u; rel < pending_count; ++rel) {
                const io::u32 idx = (pending_head + rel) % MAX_PENDING;
                if (rel == best_rel) continue;
                const io::u32 dst_idx = (pending_head + dst) % MAX_PENDING;
                if (dst_idx != idx)
                    pending[dst_idx] = pending[idx];
                ++dst;
            }
            pending_count = dst;
            pending_tail = (pending_head + pending_count) % MAX_PENDING;
            return true;
        }

        IO_NODISCARD inline bool enqueue_req(io::Endpoint to, const ge::net::ChunkRequest& req, io::u16 peer_index, io::u32 stream_slot, io::u64 now_ms) noexcept {
            if (!pending) return false;
            if (is_duplicate(to, req)) return true;
            if (pending_count >= MAX_PENDING) {
                ++stats.dropped;
                return false;
            }
            PendingReq& p = pending[pending_tail];
            p.to = to;
            p.req = req;
            p.peer_index = peer_index;
            p.stream_slot = stream_slot;
            p.enqueued_ms = now_ms;
            p.priority = request_priority(peer_index, req);
            pending_tail = (pending_tail + 1u) % MAX_PENDING;
            ++pending_count;
            return true;
        }

        inline void pop_req() noexcept {
            if (pending_count == 0u) return;
            pending_head = (pending_head + 1u) % MAX_PENDING;
            --pending_count;
        }

        IO_NODISCARD inline bool build_chunk_wire_payload(WorkerSlot& slot) noexcept {
            slot.wire_payload.clear();
            slot.send_encoding = ge::net::CHUNK_WIRE_ENCODING_RAW;
            slot.send_total_bytes = 0u;
            slot.send_part_size = 0u;
            slot.send_part_count = 0u;
            slot.send_part_cursor = 0u;
            slot.send_begin_sent = false;
            slot.send_end_sent = false;

            const ge::voxel::ChunkData& chunk = slot.chunk;
            const bool is_implicit_air = chunk.non_air_count == 0u;
            if (is_implicit_air) {
                slot.hash = ge::voxel::hash_chunk_ids_fnv1a32(chunk);
                return true;
            }

            bool has_any_state = false;
            bool can_nibble = (ge::voxel::BLOCK_COUNT <= 16u);
            if (!can_nibble) can_nibble = true;
            for (io::u32 i = 0; i < ge::voxel::CHUNK_VOLUME; ++i) {
                if (chunk.blocks[i].state != 0u)
                    has_any_state = true;
                if (can_nibble && chunk.wire_block_id_at(i) > 15u)
                    can_nibble = false;
            }

            if (has_any_state) {
                // Try compact RLE (run: u16, id: u8, state: u16) only when it wins.
                io::u32 run_start = 0u;
                while (run_start < ge::voxel::CHUNK_VOLUME) {
                    const io::u8 id = chunk.wire_block_id_at(run_start);
                    const io::u16 state = chunk.blocks[run_start].state;
                    io::u32 run_len = 1u;
                    while (run_start + run_len < ge::voxel::CHUNK_VOLUME &&
                           run_len < 65535u &&
                           chunk.wire_block_id_at(run_start + run_len) == id &&
                           chunk.blocks[run_start + run_len].state == state)
                        ++run_len;
                    const io::usize old_size = slot.wire_payload.size();
                    if (!slot.wire_payload.resize(old_size + 5u))
                        return false;
                    slot.wire_payload[old_size + 0u] = static_cast<io::u8>((run_len >> 8u) & 0xFFu);
                    slot.wire_payload[old_size + 1u] = static_cast<io::u8>(run_len & 0xFFu);
                    slot.wire_payload[old_size + 2u] = id;
                    slot.wire_payload[old_size + 3u] = static_cast<io::u8>((state >> 8u) & 0xFFu);
                    slot.wire_payload[old_size + 4u] = static_cast<io::u8>(state & 0xFFu);
                    run_start += run_len;
                }

                if (slot.wire_payload.size() < ge::net::CHUNK_WIRE_BLOCK_BYTES_RAW_STATE) {
                    slot.send_encoding = ge::net::CHUNK_WIRE_ENCODING_RLE_STATE;
                    slot.send_total_bytes = static_cast<io::u32>(slot.wire_payload.size());
                } else {
                    slot.wire_payload.clear();
                    if (!slot.wire_payload.resize(ge::net::CHUNK_WIRE_BLOCK_BYTES_RAW_STATE))
                        return false;
                    for (io::u32 i = 0u; i < ge::voxel::CHUNK_VOLUME; ++i) {
                        const io::u16 s = chunk.blocks[i].state;
                        const io::u32 off = i * 3u;
                        slot.wire_payload[off + 0u] = chunk.wire_block_id_at(i);
                        slot.wire_payload[off + 1u] = static_cast<io::u8>((s >> 8u) & 0xFFu);
                        slot.wire_payload[off + 2u] = static_cast<io::u8>(s & 0xFFu);
                    }
                    slot.send_encoding = ge::net::CHUNK_WIRE_ENCODING_RAW_STATE;
                    slot.send_total_bytes = ge::net::CHUNK_WIRE_BLOCK_BYTES_RAW_STATE;
                }
                slot.hash = ge::voxel::hash_chunk_ids_and_state_fnv1a32(chunk);
            } else if (can_nibble) {
                if (!slot.wire_payload.resize(ge::net::CHUNK_WIRE_BLOCK_BYTES_NIBBLE))
                    return false;
                for (io::u32 i = 0u; i < ge::net::CHUNK_WIRE_BLOCK_BYTES_NIBBLE; ++i) {
                    const io::u32 bi = i * 2u;
                    const io::u8 lo = chunk.wire_block_id_at(bi);
                    const io::u8 hi = chunk.wire_block_id_at(bi + 1u);
                    slot.wire_payload[i] = static_cast<io::u8>((lo & 0x0Fu) | ((hi & 0x0Fu) << 4u));
                }
                slot.send_encoding = ge::net::CHUNK_WIRE_ENCODING_NIBBLE;
                slot.send_total_bytes = ge::net::CHUNK_WIRE_BLOCK_BYTES_NIBBLE;
                slot.hash = ge::voxel::hash_chunk_ids_fnv1a32(chunk);
            } else {
                if (!slot.wire_payload.resize(ge::net::CHUNK_WIRE_BLOCK_BYTES))
                    return false;
                for (io::u32 i = 0u; i < ge::net::CHUNK_WIRE_BLOCK_BYTES; ++i)
                    slot.wire_payload[i] = chunk.wire_block_id_at(i);
                slot.send_encoding = ge::net::CHUNK_WIRE_ENCODING_RAW;
                slot.send_total_bytes = ge::net::CHUNK_WIRE_BLOCK_BYTES;
                slot.hash = ge::voxel::hash_chunk_ids_fnv1a32(chunk);
            }

            slot.send_part_size = ge::net::CHUNK_PART_BYTES;
            slot.send_part_count = ge::net::part_count_for(slot.send_total_bytes, slot.send_part_size);
            return true;
        }

        // Returns: 0 = blocked/backpressure, 1 = progressed (more to send), 2 = chunk fully sent.
        IO_NODISCARD inline io::u8 send_chunk_step(WorkerSlot& slot, io::u64 now_ms) noexcept {
            if (!slot.send_begin_sent) {
                ge::net::ChunkBegin begin{};
                begin.request_id = slot.job.req.request_id;
                begin.coord = slot.chunk.coord;
                begin.total_bytes = slot.send_total_bytes;
                begin.part_count = slot.send_part_count;
                begin.bytes_per_part = slot.send_part_size;
                begin.encoding = slot.send_encoding;
                ge::net::S2C_ChunkBegin begin_wire{};
                ge::net::encode_chunk_begin(begin, begin_wire);
                if (!loop.send_to_peer(slot.job.to, ge::net::PK_S2C_CHUNK_BEGIN, io::UdpChan::Reliable,
                                       io::byte_view{ reinterpret_cast<const io::u8*>(&begin_wire), sizeof(begin_wire) }, now_ms))
                    return 0u;
                slot.send_begin_sent = true;
                if (slot.send_part_count == 0u && slot.send_total_bytes == 0u)
                    slot.send_part_cursor = 0u;
            }

            io::u32 sent_parts = 0u;
            while (slot.send_part_cursor < slot.send_part_count && sent_parts < STREAM_PARTS_PER_TICK) {
                const io::u32 off = static_cast<io::u32>(slot.send_part_cursor) * slot.send_part_size;
                io::u16 size = slot.send_part_size;
                if (off + size > slot.send_total_bytes)
                    size = static_cast<io::u16>(slot.send_total_bytes - off);

                ge::net::ChunkPart part{};
                part.request_id = slot.job.req.request_id;
                part.part_index = slot.send_part_cursor;
                part.part_size = size;
                ge::net::S2C_ChunkPartHeader hdr{};
                ge::net::encode_chunk_part(part, hdr);
                for (io::usize k = 0; k < sizeof(hdr); ++k)
                    part_buf[k] = reinterpret_cast<const io::u8*>(&hdr)[k];
                for (io::u16 k = 0; k < size; ++k)
                    part_buf[sizeof(hdr) + k] = slot.wire_payload[off + k];
                if (!loop.send_to_peer(slot.job.to, ge::net::PK_S2C_CHUNK_PART, io::UdpChan::Reliable,
                                       io::byte_view{ part_buf, sizeof(hdr) + size }, now_ms))
                    return 0u;
                ++slot.send_part_cursor;
                ++sent_parts;
            }

            if (slot.send_part_cursor < slot.send_part_count)
                return 1u;

            if (!slot.send_end_sent) {
                ge::net::ChunkEnd end{};
                end.request_id = slot.job.req.request_id;
                end.total_bytes = slot.send_total_bytes;
                end.hash = slot.hash;
                ge::net::S2C_ChunkEnd end_wire{};
                ge::net::encode_chunk_end(end, end_wire);
                if (!loop.send_to_peer(slot.job.to, ge::net::PK_S2C_CHUNK_END, io::UdpChan::Reliable,
                                       io::byte_view{ reinterpret_cast<const io::u8*>(&end_wire), sizeof(end_wire) }, now_ms))
                    return 0u;
                slot.send_end_sent = true;
            }
            return 2u;
        }

        inline bool send_pos(io::u16 peer_index, io::u64 now_ms, io::u8 flags, io::UdpChan chan) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            PeerState& p = peers[peer_index];
            if (!p.used || !p.has_auth) return false;
            float pos_x = p.auth_x;
            float pos_y = p.auth_y;
            float pos_z = p.auth_z;
            if (player_ecs && player_ecs->alive[peer_index] != 0u) {
                pos_x = player_ecs->transform[peer_index].x;
                pos_y = player_ecs->transform[peer_index].y;
                pos_z = player_ecs->transform[peer_index].z;
            }
            ge::net::ServerPlayerPosition sample{};
            sample.server_ms = static_cast<io::u32>(now_ms - boot_ms);
            sample.x = pos_x;
            sample.y = pos_y;
            sample.z = pos_z;
            sample.flags = flags;
            ge::net::S2C_PlayerPosition wire{};
            ge::net::encode_server_player_position(sample, wire);
            const bool ok = loop.send_to_peer(p.ep, ge::net::PK_S2C_PLAYER_POSITION, chan, io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            if (ok) {
                p.last_sync_ms = now_ms;
                ++stats.pos_sync_ok;
            } else {
                if (chan == io::UdpChan::Reliable)
                    ++stats.pos_sync_fail;
                else {
                    ++stats.send_backpressure;
                    ++stats.send_backpressure_packets;
                }
            }
            return ok;
        }

        IO_NODISCARD static inline float normalize_degrees(float angle) noexcept {
            while (angle > 180.f) angle -= 360.f;
            while (angle < -180.f) angle += 360.f;
            return angle;
        }

        IO_NODISCARD static inline float wrap_radians(float angle) noexcept {
            static constexpr float PI = 3.14159265358979323846f;
            static constexpr float TWO_PI = 6.28318530717958647692f;
            while (angle > PI) angle -= TWO_PI;
            while (angle < -PI) angle += TWO_PI;
            return angle;
        }

        IO_NODISCARD static inline float sin_approx_radians(float x) noexcept {
            static constexpr float INV_PI = 0.31830988618379067153f;
            static constexpr float B = 4.f * INV_PI;
            static constexpr float C = -4.f * INV_PI * INV_PI;
            x = wrap_radians(x);
            float y = B * x + C * x * absf(x);
            const float P = 0.225f;
            y = P * (y * absf(y) - y) + y;
            if (y > 1.f) y = 1.f;
            if (y < -1.f) y = -1.f;
            return y;
        }

        IO_NODISCARD static inline float cos_approx_radians(float x) noexcept {
            static constexpr float HALF_PI = 1.57079632679489661923f;
            return sin_approx_radians(x + HALF_PI);
        }

        inline bool send_remote_pose_to_peer(io::u16 target_peer_index, io::u16 source_peer_index, io::u64 now_ms, io::UdpChan chan) noexcept {
            if (!peers) return false;
            if (target_peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            if (source_peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            if (target_peer_index == source_peer_index) return false;
            const PeerState& target = peers[target_peer_index];
            const PeerState& src = peers[source_peer_index];
            if (!target.used || !target.has_auth || target.dead) return false;
            if (!src.used || !src.has_auth || src.dead) return false;

            ge::net::RemotePlayerPoseSample sample{};
            sample.server_index = source_peer_index;
            sample.state = src.anim_state;
            sample.action_flags = src.action_flags;
            const io::u8 selected = (src.inventory.selected_hotbar < ge::item::HOTBAR_SLOT_COUNT)
                ? src.inventory.selected_hotbar : 0u;
            const ge::item::Stack& held = src.inventory.hotbar[selected];
            sample.held_item = (held.count > 0u) ? held.id : ge::item::Id::None;
            sample.x = src.auth_x;
            sample.y = src.auth_y;
            sample.z = src.auth_z;
            sample.yaw = normalize_degrees(src.look_yaw);
            sample.pitch = src.look_pitch;
            if (sample.pitch > 89.f) sample.pitch = 89.f;
            if (sample.pitch < -89.f) sample.pitch = -89.f;

            ge::net::S2C_RemotePlayerPose wire{};
            ge::net::encode_s2c_remote_player_pose(sample, wire);
            const bool ok = loop.send_to_peer(target.ep,
                                              ge::net::PK_S2C_REMOTE_PLAYER_POSE,
                                              chan,
                                              io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                                              now_ms);
            if (ok) ++stats.send_ok;
            else ++stats.send_fail;
            return ok;
        }

        inline void broadcast_remote_pose_from_peer(io::u16 source_peer_index, io::u64 now_ms) noexcept {
            if (!peers || source_peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            const PeerState& src = peers[source_peer_index];
            if (!src.used || !src.has_auth || src.dead) return;

            const float max_d2 = PLAYER_REMOTE_POSE_RADIUS * PLAYER_REMOTE_POSE_RADIUS;
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                if (i == source_peer_index) continue;
                const PeerState& dst = peers[i];
                if (!dst.used || !dst.has_auth || dst.dead) continue;
                const float dx = dst.auth_x - src.auth_x;
                const float dy = dst.auth_y - src.auth_y;
                const float dz = dst.auth_z - src.auth_z;
                const float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 > max_d2) continue;
                (void)send_remote_pose_to_peer(i, source_peer_index, now_ms, io::UdpChan::Unreliable);
            }
        }

        IO_NODISCARD static inline io::u32 tree_hash_u32(io::u32 value) noexcept {
            value ^= value >> 16u;
            value *= 0x7FEB352Du;
            value ^= value >> 15u;
            value *= 0x846CA68Bu;
            value ^= value >> 16u;
            return value;
        }

        IO_NODISCARD static inline bool is_tree_part_block(ge::voxel::BlockId id) noexcept {
            return id == ge::voxel::BlockId::Log || id == ge::voxel::BlockId::Leaves;
        }

        IO_NODISCARD static inline bool tree_can_fall_into(ge::voxel::BlockId id) noexcept {
            return id == ge::voxel::BlockId::Air || ge::voxel::is_liquid(id);
        }

        IO_NODISCARD inline bool has_active_tree_node_at(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            for (io::u32 i = 0u; i < TREE_FALL_CAP; ++i) {
                const TreeFallNode& n = tree_fall_nodes[i];
                if (!n.active) continue;
                if (n.x == wx && n.y == wy && n.z == wz) return true;
            }
            return false;
        }

        IO_NODISCARD inline bool tree_scan_contains(io::u32 count, io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            for (io::u32 i = 0u; i < count; ++i) {
                const TreeScanCell& c = tree_scan_cells[i];
                if (c.x == wx && c.y == wy && c.z == wz)
                    return true;
            }
            return false;
        }

        IO_NODISCARD inline bool alloc_tree_fall_slot(io::u32& out_idx) noexcept {
            for (io::u32 i = 0u; i < TREE_FALL_CAP; ++i) {
                const io::u32 idx = (tree_fall_cursor + i) % TREE_FALL_CAP;
                if (!tree_fall_nodes[idx].active) {
                    out_idx = idx;
                    tree_fall_cursor = (idx + 1u) % TREE_FALL_CAP;
                    return true;
                }
            }
            out_idx = 0u;
            return false;
        }

        IO_NODISCARD inline io::u32 count_free_tree_fall_slots() const noexcept {
            io::u32 free_count = 0u;
            for (io::u32 i = 0u; i < TREE_FALL_CAP; ++i) {
                if (!tree_fall_nodes[i].active)
                    ++free_count;
            }
            return free_count;
        }

        IO_NODISCARD inline bool has_vertical_log_neighbor(io::i32 x, io::i32 y, io::i32 z) const noexcept {
            return world.get_world_block(x, y - 1, z) == ge::voxel::BlockId::Log
                || world.get_world_block(x, y + 1, z) == ge::voxel::BlockId::Log;
        }

        IO_NODISCARD inline bool collect_tree_component_limited(io::i32 root_x, io::i32 root_y, io::i32 root_z,
                                                                io::u32 hard_limit,
                                                                io::u32& out_count,
                                                                bool& out_truncated) noexcept {
            // Tree detection intentionally uses a bounded flood fill:
            // - never allocates dynamic memory;
            // - hard-limits scanned nodes;
            // - keeps bounded scan extents around the cut log.
            // This guarantees stable RAM regardless of pathological block arrangements.
            out_count = 0u;
            out_truncated = false;
            if (!is_tree_part_block(world.get_world_block(root_x, root_y, root_z)))
                return false;
            if (hard_limit == 0u) return false;
            if (hard_limit > TREE_FALL_SCAN_CAP)
                hard_limit = TREE_FALL_SCAN_CAP;

            tree_scan_cells[0] = TreeScanCell{ root_x, root_y, root_z };
            out_count = 1u;
            io::u32 head = 0u;
            static constexpr io::i32 TREE_SCAN_RADIUS_XZ = ge::worldgen::SimplexTerrain::TREE_MAX_RADIUS_XZ + 10;
            static constexpr io::i32 TREE_SCAN_RADIUS_Y_ABOVE = 52;
            static constexpr io::i32 TREE_SCAN_RADIUS_Y_BELOW = 20;
            while (head < out_count) {
                const TreeScanCell cur = tree_scan_cells[head++];

                for (io::i32 dz = -1; dz <= 1; ++dz) {
                    for (io::i32 dy = -1; dy <= 1; ++dy) {
                        for (io::i32 dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0 && dz == 0) continue;
                            const io::i32 nx = cur.x + dx;
                            const io::i32 ny = cur.y + dy;
                            const io::i32 nz = cur.z + dz;
                            if (abs_i32(nx - root_x) > TREE_SCAN_RADIUS_XZ) continue;
                            if (abs_i32(nz - root_z) > TREE_SCAN_RADIUS_XZ) continue;
                            if (ny > root_y + TREE_SCAN_RADIUS_Y_ABOVE) continue;
                            if (ny < root_y - TREE_SCAN_RADIUS_Y_BELOW) continue;
                            if (!is_tree_part_block(world.get_world_block(nx, ny, nz)))
                                continue;
                            if (tree_scan_contains(out_count, nx, ny, nz))
                                continue;
                            if (out_count >= hard_limit) {
                                out_truncated = true;
                                return true;
                            }
                            tree_scan_cells[out_count++] = TreeScanCell{ nx, ny, nz };
                        }
                    }
                }
            }
            return out_count > 0u;
        }

        IO_NODISCARD inline bool generated_layout_contains_log(const ge::worldgen::SimplexTerrain::TreeBlock* blocks,
                                                               io::u32 count,
                                                               io::i32 x, io::i32 y, io::i32 z) const noexcept {
            for (io::u32 i = 0u; i < count; ++i) {
                const ge::worldgen::SimplexTerrain::TreeBlock& b = blocks[i];
                if (b.id != ge::voxel::BlockId::Log) continue;
                if (b.x == x && b.y == y && b.z == z) return true;
            }
            return false;
        }

        IO_NODISCARD inline bool find_generated_large_tree_for_log(io::i32 root_x, io::i32 root_y, io::i32 root_z,
                                                                    io::u32& out_generated_count) noexcept {
            out_generated_count = 0u;
            if (!terrain) return false;

            // Blue-noise re-generation check:
            // We reconstruct nearby worldgen tree anchors and regenerate each layout deterministically.
            // A large tree is accepted only if the broken log coordinate exactly matches a generated log block
            // (X, Y, Z all match). This prevents player-made mega-structures from triggering mass tree fall.
            ge::voxel::ChunkCoord min_cc{};
            ge::voxel::ChunkCoord max_cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            const io::i32 r = ge::worldgen::SimplexTerrain::TREE_MAX_RADIUS_XZ + 2;
            ge::voxel::split_world_coord(root_x - r, 0, root_z - r, min_cc, lx, ly, lz);
            ge::voxel::split_world_coord(root_x + r, 0, root_z + r, max_cc, lx, ly, lz);

            for (io::i32 anchor_cz = min_cc.z - 1; anchor_cz <= max_cc.z + 1; ++anchor_cz) {
                for (io::i32 anchor_cx = min_cc.x - 1; anchor_cx <= max_cc.x + 1; ++anchor_cx) {
                    for (io::u32 attempt = 0u; attempt < 4u; ++attempt) {
                        io::i32 gx = 0, gz = 0;
                        io::u32 seed = 0u;
                        if (!terrain->tree_anchor_candidate(anchor_cx, anchor_cz, attempt, gx, gz, seed))
                            continue;

                        io::u32 block_count = 0u;
                        bool is_large = false;
                        terrain_lock.lock();
                        const bool built = terrain->build_tree_layout(gx, gz, seed,
                                                                      tree_generated_blocks, TREE_GENERATED_BLOCK_CAP,
                                                                      block_count, is_large);
                        terrain_lock.unlock();
                        if (!built)
                            continue;
                        if (!is_large || block_count <= TREE_SMALL_COMPONENT_CAP)
                            continue;
                        if (!generated_layout_contains_log(tree_generated_blocks, block_count, root_x, root_y, root_z))
                            continue;
                        out_generated_count = block_count;
                        return true;
                    }
                }
            }
            return false;
        }

        IO_NODISCARD inline bool try_begin_tree_fall_on_log_break(io::i32 root_x, io::i32 root_y, io::i32 root_z, io::u64 now_ms) noexcept {
            // Tree-fall classifier (authoritative server, memory-bounded):
            // 1) Trigger only when a broken log has vertical continuity (log above or below).
            // 2) Try cheap bounded component scan first (<=128 nodes). This path handles:
            //    - naturally generated small trees,
            //    - player-built small trees/saplings,
            //    without any blue-noise revalidation.
            // 3) If component exceeds 128 nodes, treat it as "large candidate" and require
            //    deterministic worldgen re-generation from blue-noise anchors. The broken log
            //    must exactly match a regenerated log block at (X,Y,Z).
            // 4) Only validated blocks are scheduled into falling simulation slots.
            //    This prevents accidental chain-fall through leaf bridges and keeps RAM bounded.
            if (world.get_world_block(root_x, root_y, root_z) != ge::voxel::BlockId::Log)
                return false;
            if (!has_vertical_log_neighbor(root_x, root_y, root_z))
                return false;

            io::u32 part_count = 0u;
            bool part_truncated = false;
            if (!collect_tree_component_limited(root_x, root_y, root_z,
                                                TREE_SMALL_COMPONENT_CAP + 1u,
                                                part_count, part_truncated))
                return false;

            const bool use_small_component = (!part_truncated && part_count <= TREE_SMALL_COMPONENT_CAP);

            io::u32 required_slots = 0u;
            io::u32 generated_count = 0u;

            if (use_small_component) {
                for (io::u32 i = 0u; i < part_count; ++i) {
                    const TreeScanCell c = tree_scan_cells[i];
                    if (c.x == root_x && c.y == root_y && c.z == root_z)
                        continue;
                    if (has_active_tree_node_at(c.x, c.y, c.z))
                        continue;
                    const ge::voxel::BlockState bs = world.get_world_state(c.x, c.y, c.z);
                    const ge::voxel::BlockId id = (bs.id < ge::voxel::BLOCK_COUNT) ? static_cast<ge::voxel::BlockId>(bs.id) : ge::voxel::BlockId::Air;
                    if (is_tree_part_block(id))
                        ++required_slots;
                }
            } else {
                if (!find_generated_large_tree_for_log(root_x, root_y, root_z, generated_count))
                    return false;
                for (io::u32 i = 0u; i < generated_count; ++i) {
                    const ge::worldgen::SimplexTerrain::TreeBlock& b = tree_generated_blocks[i];
                    if (!is_tree_part_block(b.id))
                        continue;
                    if (b.x == root_x && b.y == root_y && b.z == root_z)
                        continue;
                    if (has_active_tree_node_at(b.x, b.y, b.z))
                        continue;
                    ge::voxel::ChunkCoord cc{};
                    io::u32 lx = 0u, ly = 0u, lz = 0u;
                    ge::voxel::split_world_coord(b.x, b.y, b.z, cc, lx, ly, lz);
                    if (!ensure_world_chunk(cc))
                        continue;
                    const ge::voxel::BlockState bs = world.get_world_state(b.x, b.y, b.z);
                    const ge::voxel::BlockId id = (bs.id < ge::voxel::BLOCK_COUNT)
                        ? static_cast<ge::voxel::BlockId>(bs.id)
                        : ge::voxel::BlockId::Air;
                    if (id == b.id)
                        ++required_slots;
                }
            }

            if (required_slots == 0u) return false;
            if (required_slots > count_free_tree_fall_slots()) return false;

            bool scheduled_any = false;
            if (use_small_component) {
                for (io::u32 i = 0u; i < part_count; ++i) {
                    const TreeScanCell c = tree_scan_cells[i];
                    if (c.x == root_x && c.y == root_y && c.z == root_z)
                        continue;
                    if (has_active_tree_node_at(c.x, c.y, c.z))
                        continue;
                    const ge::voxel::BlockState bs = world.get_world_state(c.x, c.y, c.z);
                    const ge::voxel::BlockId id = (bs.id < ge::voxel::BLOCK_COUNT) ? static_cast<ge::voxel::BlockId>(bs.id) : ge::voxel::BlockId::Air;
                    if (!is_tree_part_block(id))
                        continue;
                    io::u32 slot = 0u;
                    if (!alloc_tree_fall_slot(slot))
                        return false;
                    TreeFallNode& n = tree_fall_nodes[slot];
                    n = {};
                    n.active = true;
                    n.id = id;
                    n.state = bs.state;
                    n.x = c.x;
                    n.y = c.y;
                    n.z = c.z;
                    n.seed = tree_hash_u32(static_cast<io::u32>(c.x) ^ (static_cast<io::u32>(c.y) * 131u) ^ (static_cast<io::u32>(c.z) * 8191u));
                    n.next_step_ms = now_ms + 40u + static_cast<io::u64>(n.seed & 63u);
                    n.settle_until_ms = 0u;
                    n.phase = 0u;
                    scheduled_any = true;
                }
            } else {
                // Large-tree path:
                // - only worldgen-regenerated blocks are considered;
                // - each block is validated against currently loaded world state;
                // - this avoids accidental chain-falls through leaf bridges between separate trees.
                for (io::u32 i = 0u; i < generated_count; ++i) {
                    const ge::worldgen::SimplexTerrain::TreeBlock& b = tree_generated_blocks[i];
                    if (!is_tree_part_block(b.id))
                        continue;
                    if (b.x == root_x && b.y == root_y && b.z == root_z)
                        continue;
                    if (has_active_tree_node_at(b.x, b.y, b.z))
                        continue;
                    ge::voxel::ChunkCoord cc{};
                    io::u32 lx = 0u, ly = 0u, lz = 0u;
                    ge::voxel::split_world_coord(b.x, b.y, b.z, cc, lx, ly, lz);
                    if (!ensure_world_chunk(cc))
                        continue;
                    const ge::voxel::BlockState bs = world.get_world_state(b.x, b.y, b.z);
                    const ge::voxel::BlockId id = (bs.id < ge::voxel::BLOCK_COUNT)
                        ? static_cast<ge::voxel::BlockId>(bs.id)
                        : ge::voxel::BlockId::Air;
                    if (id != b.id)
                        continue;
                    io::u32 slot = 0u;
                    if (!alloc_tree_fall_slot(slot))
                        return false;
                    TreeFallNode& n = tree_fall_nodes[slot];
                    n = {};
                    n.active = true;
                    n.id = id;
                    n.state = bs.state;
                    n.x = b.x;
                    n.y = b.y;
                    n.z = b.z;
                    n.seed = tree_hash_u32(static_cast<io::u32>(b.x) ^ (static_cast<io::u32>(b.y) * 131u) ^ (static_cast<io::u32>(b.z) * 8191u));
                    n.next_step_ms = now_ms + 40u + static_cast<io::u64>(n.seed & 63u);
                    n.settle_until_ms = 0u;
                    n.phase = 0u;
                    scheduled_any = true;
                }
            }
            return scheduled_any;
        }

        inline void spawn_tree_fall_drop(const TreeFallNode& n, io::u64 now_ms) noexcept {
            const ge::item::Id drop_id = ge::item::block_drop_for(n.id);
            if (drop_id == ge::item::Id::None) return;
            if (n.id == ge::voxel::BlockId::Leaves) {
                if ((n.seed % 100u) >= TREE_FALL_LEAF_DROP_CHANCE_PERCENT)
                    return;
            }
            ge::item::Stack drop = ge::item::make_stack(drop_id, 1u);
            const float jx = static_cast<float>((n.seed & 7u)) * 0.015f - 0.045f;
            const float jz = static_cast<float>((n.seed >> 3u) & 7u) * 0.015f - 0.045f;
            (void)spawn_item_actor(drop,
                                   static_cast<float>(n.x) + 0.5f + jx,
                                   static_cast<float>(n.y) + 0.35f,
                                   static_cast<float>(n.z) + 0.5f + jz,
                                   jx * 4.0f, 0.45f, jz * 4.0f,
                                   now_ms);
        }

        inline void update_tree_fall(io::u64 now_ms) noexcept {
            for (io::u32 i = 0u; i < TREE_FALL_CAP; ++i) {
                TreeFallNode& n = tree_fall_nodes[i];
                if (!n.active) continue;
                if (now_ms < n.next_step_ms) continue;

                // Do not let tree-fall slots leak forever when chunks get pruned:
                // - if the node chunk is cold and unloaded, retire the node;
                // - if it is still hot, reload it and continue simulation.
                ge::voxel::ChunkCoord node_cc{};
                io::u32 nlx = 0u, nly = 0u, nlz = 0u;
                ge::voxel::split_world_coord(n.x, n.y, n.z, node_cc, nlx, nly, nlz);
                if (!world.find_chunk(node_cc)) {
                    if (!is_chunk_hot_any(node_cc)) {
                        n.active = false;
                        continue;
                    }
                    if (!ensure_world_chunk(node_cc)) {
                        n.active = false;
                        continue;
                    }
                }

                if (n.phase == 0u) {
                    ge::voxel::BlockId cur_id = ge::voxel::BlockId::Air;
                    io::u16 cur_state = 0u;
                    if (!try_get_loaded_world_state(n.x, n.y, n.z, cur_id, cur_state)) {
                        n.next_step_ms = now_ms + TREE_FALL_STEP_MS;
                        continue;
                    }
                    (void)cur_state;
                    if (cur_id != n.id) {
                        n.active = false;
                        continue;
                    }
                    const io::i32 ny = n.y - 1;
                    ge::voxel::BlockId below = ge::voxel::BlockId::Air;
                    io::u16 below_state = 0u;
                    if (!try_get_loaded_world_state(n.x, ny, n.z, below, below_state)) {
                        n.next_step_ms = now_ms + TREE_FALL_STEP_MS;
                        continue;
                    }
                    (void)below_state;
                    if (tree_can_fall_into(below)) {
                        ge::net::BlockEdit from{};
                        from.wx = n.x;
                        from.wy = n.y;
                        from.wz = n.z;
                        from.block_id = ge::voxel::block_index(ge::voxel::BlockId::Air);
                        from.state = 0u;
                        bool removed = false;
                        if (apply_block_edit_world(from, false)) {
                            broadcast_block_edit(from, now_ms);
                            removed = true;
                        }

                        ge::net::BlockEdit to{};
                        to.wx = n.x;
                        to.wy = ny;
                        to.wz = n.z;
                        to.block_id = ge::voxel::block_index(n.id);
                        to.state = n.state;
                        if (apply_block_edit_world(to, false)) {
                            broadcast_block_edit(to, now_ms);
                            n.y = ny;
                        } else {
                            if (removed) {
                                ge::net::BlockEdit restore{};
                                restore.wx = n.x;
                                restore.wy = n.y;
                                restore.wz = n.z;
                                restore.block_id = ge::voxel::block_index(n.id);
                                restore.state = n.state;
                                if (apply_block_edit_world(restore, false))
                                    broadcast_block_edit(restore, now_ms);
                            }
                            n.next_step_ms = now_ms + TREE_FALL_STEP_MS;
                            continue;
                        }
                        n.next_step_ms = now_ms + TREE_FALL_STEP_MS + static_cast<io::u64>(n.seed & 15u);
                    } else {
                        // If the supporting block is another active falling tree node, wait instead of
                        // prematurely settling this block (prevents floating/stuck canopy pieces).
                        if (has_active_tree_node_at(n.x, ny, n.z)) {
                            n.next_step_ms = now_ms + TREE_FALL_STEP_MS;
                            continue;
                        }
                        n.phase = 1u;
                        const io::u32 base = (n.id == ge::voxel::BlockId::Leaves) ? TREE_FALL_SETTLE_LEAF_MS : TREE_FALL_SETTLE_LOG_MS;
                        n.settle_until_ms = now_ms + base + static_cast<io::u64>(n.seed % 550u);
                        n.next_step_ms = n.settle_until_ms;
                    }
                    continue;
                }

                if (n.phase == 1u && now_ms >= n.settle_until_ms) {
                    if (world.get_world_block(n.x, n.y, n.z) == n.id) {
                        ge::net::BlockEdit to_air{};
                        to_air.wx = n.x;
                        to_air.wy = n.y;
                        to_air.wz = n.z;
                        to_air.block_id = ge::voxel::block_index(ge::voxel::BlockId::Air);
                        to_air.state = 0u;
                        if (apply_block_edit_world(to_air, false))
                            broadcast_block_edit(to_air, now_ms);
                    }
                    spawn_tree_fall_drop(n, now_ms);
                    n.active = false;
                }
            }
        }

        IO_NODISCARD inline bool melee_los_clear(float ax, float ay, float az,
                                                 float bx, float by, float bz) const noexcept {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;
            const float span = absf(dx) + absf(dy) + absf(dz);
            if (span <= 0.000001f) return true;
            io::i32 steps = static_cast<io::i32>(span / 0.25f);
            if (steps < 1) steps = 1;
            for (io::i32 i = 1; i < steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const io::i32 wx = floor_to_i32(ax + dx * t);
                const io::i32 wy = floor_to_i32(ay + dy * t);
                const io::i32 wz = floor_to_i32(az + dz * t);
                const ge::voxel::BlockId id = world.get_world_block(wx, wy, wz);
                if (id == ge::voxel::BlockId::Air) continue;
                if (ge::voxel::is_liquid(id)) continue;
                if (is_book_anchor(id)) continue;
                if (ge::voxel::is_solid(id))
                    return false;
            }
            return true;
        }

        IO_NODISCARD static inline bool spell_is_supported(ge::item::Id id) noexcept {
            switch (id) {
            case ge::item::Id::SpellBolt:
            case ge::item::Id::SpellDig:
            case ge::item::Id::SpellBurst:
            case ge::item::Id::SpellBeam:
            case ge::item::Id::SpellOrb:
            case ge::item::Id::SpellMine:
            case ge::item::Id::SpellShieldPulse:
            case ge::item::Id::SpellMark:
            case ge::item::Id::SpellPull:
            case ge::item::Id::SpellBlinkStep:
                return true;
            default:
                return false;
            }
        }

        IO_NODISCARD static inline bool spell_is_projectile(ge::item::Id id) noexcept {
            return id == ge::item::Id::SpellBolt || id == ge::item::Id::SpellBurst || id == ge::item::Id::SpellOrb;
        }

        IO_NODISCARD static inline io::u16 spell_base_damage(ge::item::Id id) noexcept {
            switch (id) {
            case ge::item::Id::SpellBolt: return 12u;
            case ge::item::Id::SpellBurst: return 7u;
            case ge::item::Id::SpellBeam: return 6u;
            case ge::item::Id::SpellOrb: return 14u;
            case ge::item::Id::SpellMine: return 18u;
            case ge::item::Id::SpellShieldPulse: return 4u;
            default: return 0u;
            }
        }

        IO_NODISCARD static inline io::u32 spell_extra_cast_ms(ge::item::Id id) noexcept {
            switch (id) {
            case ge::item::Id::SpellBurst: return 120u;
            case ge::item::Id::SpellBeam: return 90u;
            case ge::item::Id::SpellOrb: return 200u;
            case ge::item::Id::SpellMine: return 240u;
            case ge::item::Id::SpellShieldPulse: return 170u;
            case ge::item::Id::SpellMark: return 120u;
            case ge::item::Id::SpellPull: return 150u;
            case ge::item::Id::SpellBlinkStep: return 200u;
            default: return 0u;
            }
        }

        IO_NODISCARD static inline io::u32 spell_ttl_ms(ge::item::Id id) noexcept {
            switch (id) {
            case ge::item::Id::SpellBeam: return PLAYER_SPELL_BEAM_TTL_MS;
            case ge::item::Id::SpellMark: return PLAYER_SPELL_MARK_TTL_MS;
            case ge::item::Id::SpellMine: return 120000u;
            case ge::item::Id::SpellShieldPulse:
            case ge::item::Id::SpellPull:
            case ge::item::Id::SpellBlinkStep:
                return 220u;
            default:
                return PLAYER_SPELL_PROJECTILE_TTL_MS;
            }
        }

        static inline void spell_forward_from_yaw_pitch(float yaw_deg, float pitch_deg,
                                                        float& out_x, float& out_y, float& out_z) noexcept {
            yaw_deg = normalize_degrees(yaw_deg);
            if (pitch_deg > 89.f) pitch_deg = 89.f;
            if (pitch_deg < -89.f) pitch_deg = -89.f;
            const float yaw = yaw_deg * 0.01745329251994329577f;
            const float pitch = pitch_deg * 0.01745329251994329577f;
            const float cos_pitch = cos_approx_radians(pitch);
            out_x = cos_approx_radians(yaw) * cos_pitch;
            out_y = sin_approx_radians(pitch);
            out_z = sin_approx_radians(yaw) * cos_pitch;
            const float len = absf(out_x) + absf(out_y) + absf(out_z);
            if (len > 0.000001f) {
                const float inv = 1.f / len;
                out_x *= inv;
                out_y *= inv;
                out_z *= inv;
            } else {
                out_x = 0.f;
                out_y = 0.f;
                out_z = -1.f;
            }
        }

        IO_NODISCARD inline bool spell_raycast_first_solid(float ox, float oy, float oz,
                                                           float dx, float dy, float dz,
                                                           float max_dist, float step,
                                                           io::i32& out_wx, io::i32& out_wy, io::i32& out_wz,
                                                           io::i32& out_prev_x, io::i32& out_prev_y, io::i32& out_prev_z) const noexcept {
            out_wx = out_wy = out_wz = 0;
            out_prev_x = floor_to_i32(ox);
            out_prev_y = floor_to_i32(oy);
            out_prev_z = floor_to_i32(oz);
            if (max_dist <= 0.f || step <= 0.f) return false;

            io::i32 prev_x = out_prev_x;
            io::i32 prev_y = out_prev_y;
            io::i32 prev_z = out_prev_z;
            for (float t = step; t <= max_dist; t += step) {
                const io::i32 wx = floor_to_i32(ox + dx * t);
                const io::i32 wy = floor_to_i32(oy + dy * t);
                const io::i32 wz = floor_to_i32(oz + dz * t);
                if (wx == prev_x && wy == prev_y && wz == prev_z)
                    continue;
                const ge::voxel::BlockId id = world.get_world_block(wx, wy, wz);
                if (id != ge::voxel::BlockId::Air && !ge::voxel::is_liquid(id)) {
                    out_wx = wx;
                    out_wy = wy;
                    out_wz = wz;
                    out_prev_x = prev_x;
                    out_prev_y = prev_y;
                    out_prev_z = prev_z;
                    return true;
                }
                prev_x = wx;
                prev_y = wy;
                prev_z = wz;
            }
            return false;
        }

        IO_NODISCARD inline io::i32 spawn_spell_actor(ge::item::Id spell_id,
                                                       io::u8 owner_peer,
                                                       float x, float y, float z,
                                                       float vx, float vy, float vz,
                                                       io::u32 ttl_ms,
                                                       float radius,
                                                       float power,
                                                       io::u8 flags,
                                                       io::u64 now_ms) noexcept {
            if (!world_actor_ecs) return -1;
            ActorEcs& ecs = *world_actor_ecs;
            const io::i32 slot = ecs.Spawn(ge::net::WORLD_ACTOR_MODEL_SPELL, ge::net::WORLD_ACTOR_MODE_ENTITY);
            if (slot < 0) return -1;
            const io::u32 i = static_cast<io::u32>(slot);
            ecs.transform[i].x = x;
            ecs.transform[i].y = y;
            ecs.transform[i].z = z;
            ecs.velocity[i].x = vx;
            ecs.velocity[i].y = vy;
            ecs.velocity[i].z = vz;
            ecs.mob_state[i].logical = ge::ecs::MobLogicState::Idle;
            ecs.mob_state[i].net_state = static_cast<io::u8>(spell_id);
            ecs.mob_state[i].net_anim = flags;
            ecs.item_drop[i] = {};
            ecs.spell_fx[i].spell = spell_id;
            ecs.spell_fx[i].owner_peer = owner_peer;
            ecs.spell_fx[i].flags = flags;
            ecs.spell_fx[i].ttl_ms = ttl_ms;
            ecs.spell_fx[i].radius = radius;
            ecs.spell_fx[i].power = power;
            ecs.net_sync[i].active = true;
            ecs.net_sync[i].dirty = true;
            ecs.net_sync[i].last_update_ms = now_ms;
            return slot;
        }

        IO_NODISCARD inline bool apply_spell_damage_to_peer(io::u16 attacker_idx,
                                                            io::u16 victim_idx,
                                                            io::u16 damage,
                                                            float knockback_xz,
                                                            float knockback_y,
                                                            io::u64 now_ms) noexcept {
            if (!peers) return false;
            if (attacker_idx >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            if (victim_idx >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            if (attacker_idx == victim_idx) return false;
            PeerState& attacker = peers[attacker_idx];
            PeerState& victim = peers[victim_idx];
            if (!attacker.used || !attacker.has_auth || attacker.dead) return false;
            if (!victim.used || !victim.has_auth || victim.dead) return false;
            if (victim.use_fly) return false;
            if (damage > victim.hp) damage = victim.hp;
            if (damage == 0u) return false;

            if (knockback_xz > 0.0001f) {
                const float kx0 = victim.auth_x - attacker.auth_x;
                const float kz0 = victim.auth_z - attacker.auth_z;
                const float klen = absf(kx0) + absf(kz0);
                if (klen > 0.000001f) {
                    const float inv = 1.f / klen;
                    const float kx = kx0 * inv;
                    const float kz = kz0 * inv;
                    float nx = victim.auth_x + kx * knockback_xz;
                    float ny = victim.auth_y + knockback_y;
                    float nz = victim.auth_z + kz * knockback_xz;
                    float rx = nx, ry = ny, rz = nz;
                    if (!victim.use_noclip && !try_resolve_player_inside_solid(nx, ny, nz, rx, ry, rz, victim.crawling)) {
                        rx = victim.auth_x;
                        ry = victim.auth_y;
                        rz = victim.auth_z;
                    }
                    victim.auth_x = rx;
                    victim.auth_y = ry;
                    victim.auth_z = rz;
                    victim.pending_x = rx;
                    victim.pending_y = ry;
                    victim.pending_z = rz;
                    victim.has_pending = false;
                    victim.auth_ms = now_ms;
                    victim.pending_ms = now_ms;
                    sync_player_ecs_from_peer(victim_idx);
                    (void)send_pos(victim_idx, now_ms, ge::net::PLAYER_POS_FLAG_CORRECTION, io::UdpChan::Reliable);
                }
            }

            victim.hp = static_cast<io::u16>(victim.hp - damage);
            if (victim.hp == 0u) {
                kill_peer(victim_idx, victim, now_ms, static_cast<float>(damage), 0.f, ge::net::DeathReason::Unknown);
            } else {
                (void)send_health(victim_idx, now_ms, static_cast<float>(damage), 0.f, io::UdpChan::Reliable);
            }
            broadcast_remote_pose_from_peer(attacker_idx, now_ms);
            broadcast_remote_pose_from_peer(victim_idx, now_ms);
            return true;
        }

        IO_NODISCARD inline bool spell_break_block(io::u16 peer_index,
                                                   io::i32 wx, io::i32 wy, io::i32 wz,
                                                   bool allow_hard,
                                                   io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            if (!ensure_world_chunk(cc))
                return false;
            const ge::voxel::BlockId prev_id = world.get_world_block(wx, wy, wz);
            if (prev_id == ge::voxel::BlockId::Air || ge::voxel::is_liquid(prev_id))
                return false;
            if (!allow_hard && ge::build::hardness_of(block_build_profile, prev_id) == ge::build::BlockHardness::Hard)
                return false;

            ge::net::BlockEdit edit{};
            edit.wx = wx;
            edit.wy = wy;
            edit.wz = wz;
            edit.block_id = ge::voxel::block_index(ge::voxel::BlockId::Air);
            edit.state = 0u;
            if (!apply_block_edit_world(edit))
                return false;
            broadcast_block_edit(edit, now_ms);

            const ge::item::Id drop_id = ge::item::block_drop_for(prev_id);
            if (drop_id != ge::item::Id::None) {
                ge::item::Stack drop = ge::item::make_stack(drop_id, 1u);
                const float jitter = static_cast<float>((wx * 13 + wz * 7) & 3) * 0.05f;
                if (!spawn_item_actor(drop,
                                      static_cast<float>(wx) + 0.5f,
                                      static_cast<float>(wy) + 0.35f,
                                      static_cast<float>(wz) + 0.5f,
                                      0.12f + jitter, 2.6f, -0.10f - jitter,
                                      now_ms)) {
                    (void)give_item_to_peer(peer_index, drop, now_ms);
                }
            }
            return true;
        }

        IO_NODISCARD inline bool cast_spell_from_ward(io::u16 peer_index,
                                                      ge::item::Id spell_id,
                                                      float yaw,
                                                      float pitch,
                                                      io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS))
                return false;
            PeerState& caster = peers[peer_index];
            if (!caster.used || !caster.has_auth || caster.dead)
                return false;

            float dx = 0.f, dy = 0.f, dz = -1.f;
            spell_forward_from_yaw_pitch(yaw, pitch, dx, dy, dz);
            const float eye_x = caster.auth_x;
            const float eye_y = caster.auth_y - 0.04f;
            const float eye_z = caster.auth_z;

            switch (spell_id) {
            case ge::item::Id::SpellBolt: {
                const float speed = 15.5f;
                return spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                         eye_x + dx * 0.65f, eye_y + dy * 0.65f, eye_z + dz * 0.65f,
                                         dx * speed, dy * speed, dz * speed,
                                         spell_ttl_ms(spell_id), 0.35f,
                                         static_cast<float>(spell_base_damage(spell_id)), 0u, now_ms) >= 0;
            }
            case ge::item::Id::SpellBurst: {
                bool spawned = false;
                for (io::u32 i = 0u; i < 5u; ++i) {
                    const io::u32 seed = ward_hash32(static_cast<io::u32>(now_ms) + 97u * i + static_cast<io::u32>(peer_index) * 131u);
                    const float yaw_off = (static_cast<float>(seed & 255u) / 255.f - 0.5f) * 16.f;
                    const float pitch_off = (static_cast<float>((seed >> 8u) & 255u) / 255.f - 0.5f) * 12.f;
                    float sdx = 0.f, sdy = 0.f, sdz = -1.f;
                    spell_forward_from_yaw_pitch(yaw + yaw_off, pitch + pitch_off, sdx, sdy, sdz);
                    const float speed = 13.5f;
                    if (spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                          eye_x + sdx * 0.60f, eye_y + sdy * 0.60f, eye_z + sdz * 0.60f,
                                          sdx * speed, sdy * speed, sdz * speed,
                                          spell_ttl_ms(spell_id), 0.28f,
                                          static_cast<float>(spell_base_damage(spell_id)), 0u, now_ms) >= 0) {
                        spawned = true;
                    }
                }
                return spawned;
            }
            case ge::item::Id::SpellOrb: {
                const float speed = 6.0f;
                return spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                         eye_x + dx * 0.70f, eye_y + dy * 0.70f, eye_z + dz * 0.70f,
                                         dx * speed, dy * speed, dz * speed,
                                         spell_ttl_ms(spell_id), 0.55f,
                                         static_cast<float>(spell_base_damage(spell_id)), 0u, now_ms) >= 0;
            }
            case ge::item::Id::SpellDig: {
                bool carved_any = false;
                io::i32 prev_wx = floor_to_i32(eye_x);
                io::i32 prev_wy = floor_to_i32(eye_y);
                io::i32 prev_wz = floor_to_i32(eye_z);
                io::u32 carved = 0u;
                for (float t = 0.45f; t <= 7.0f; t += 0.45f) {
                    const io::i32 wx = floor_to_i32(eye_x + dx * t);
                    const io::i32 wy = floor_to_i32(eye_y + dy * t);
                    const io::i32 wz = floor_to_i32(eye_z + dz * t);
                    if (wx == prev_wx && wy == prev_wy && wz == prev_wz)
                        continue;
                    prev_wx = wx; prev_wy = wy; prev_wz = wz;
                    const ge::voxel::BlockId id = world.get_world_block(wx, wy, wz);
                    if (id == ge::voxel::BlockId::Air || ge::voxel::is_liquid(id))
                        continue;
                    if (!spell_break_block(peer_index, wx, wy, wz, false, now_ms))
                        break;
                    carved_any = true;
                    ++carved;
                    if (carved >= 3u)
                        break;
                }
                if (carved_any) {
                    (void)spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                            eye_x + dx * 1.2f, eye_y + dy * 1.2f, eye_z + dz * 1.2f,
                                            0.f, 0.f, 0.f, 220u, 0.50f, 0.f, 0u, now_ms);
                }
                return carved_any;
            }
            case ge::item::Id::SpellBeam: {
                bool hit_any = false;
                const float range = PLAYER_SPELL_REACH;
                const float step = 0.35f;
                float hit_x = eye_x + dx * range;
                float hit_y = eye_y + dy * range;
                float hit_z = eye_z + dz * range;
                for (float t = 0.25f; t <= range; t += step) {
                    const float px = eye_x + dx * t;
                    const float py = eye_y + dy * t;
                    const float pz = eye_z + dz * t;
                    const ge::voxel::BlockId bid = world.get_world_block(floor_to_i32(px), floor_to_i32(py), floor_to_i32(pz));
                    if (bid != ge::voxel::BlockId::Air && !ge::voxel::is_liquid(bid)) {
                        hit_x = px; hit_y = py; hit_z = pz;
                        break;
                    }
                    for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                        if (i == peer_index) continue;
                        const PeerState& p = peers[i];
                        if (!p.used || !p.has_auth || p.dead) continue;
                        const float qx = p.auth_x - px;
                        const float qy = (p.auth_y - 0.85f) - py;
                        const float qz = p.auth_z - pz;
                        if (qx * qx + qy * qy + qz * qz > 0.55f * 0.55f) continue;
                        hit_x = p.auth_x;
                        hit_y = p.auth_y - 0.85f;
                        hit_z = p.auth_z;
                        hit_any |= apply_spell_damage_to_peer(peer_index, i, spell_base_damage(spell_id), 0.28f, 0.04f, now_ms);
                        t = range + step;
                        break;
                    }
                }
                (void)spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                        hit_x, hit_y, hit_z, 0.f, 0.f, 0.f,
                                        spell_ttl_ms(spell_id), 0.45f, 0.f, 0u, now_ms);
                return hit_any || true;
            }
            case ge::item::Id::SpellMine: {
                io::i32 hit_x = 0, hit_y = 0, hit_z = 0;
                io::i32 prev_x = 0, prev_y = 0, prev_z = 0;
                if (!spell_raycast_first_solid(eye_x, eye_y, eye_z, dx, dy, dz, 7.0f, 0.20f,
                                               hit_x, hit_y, hit_z, prev_x, prev_y, prev_z)) {
                    prev_x = floor_to_i32(eye_x + dx * 2.2f);
                    prev_y = floor_to_i32(eye_y + dy * 2.2f);
                    prev_z = floor_to_i32(eye_z + dz * 2.2f);
                }
                const float mx = static_cast<float>(prev_x) + 0.5f;
                const float my = static_cast<float>(prev_y) + 0.18f;
                const float mz = static_cast<float>(prev_z) + 0.5f;
                return spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                         mx, my, mz, 0.f, 0.f, 0.f,
                                         spell_ttl_ms(spell_id), PLAYER_SPELL_MINE_TRIGGER_RADIUS,
                                         static_cast<float>(PLAYER_SPELL_MINE_ARM_MS), 0u, now_ms) >= 0;
            }
            case ge::item::Id::SpellShieldPulse: {
                bool affected = false;
                const float radius2 = PLAYER_SPELL_SHIELD_RADIUS * PLAYER_SPELL_SHIELD_RADIUS;
                for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                    if (i == peer_index) continue;
                    const PeerState& p = peers[i];
                    if (!p.used || !p.has_auth || p.dead) continue;
                    const float qx = p.auth_x - caster.auth_x;
                    const float qy = (p.auth_y - caster.auth_y);
                    const float qz = p.auth_z - caster.auth_z;
                    if (qx * qx + qy * qy + qz * qz > radius2) continue;
                    affected |= apply_spell_damage_to_peer(peer_index, i, spell_base_damage(spell_id), 2.2f, 0.26f, now_ms);
                }
                if (world_actor_ecs) {
                    ActorEcs& ecs = *world_actor_ecs;
                    for (io::u32 i = 0u; i < WORLD_ACTOR_CAP; ++i) {
                        if (ecs.alive[i] == 0u || !ecs.net_sync[i].active) continue;
                        if (ecs.identity[i].model != ge::net::WORLD_ACTOR_MODEL_SPELL) continue;
                        if (!spell_is_projectile(ecs.spell_fx[i].spell)) continue;
                        if (ecs.spell_fx[i].owner_peer == static_cast<io::u8>(peer_index)) continue;
                        const float qx = ecs.transform[i].x - caster.auth_x;
                        const float qy = ecs.transform[i].y - (caster.auth_y - 0.2f);
                        const float qz = ecs.transform[i].z - caster.auth_z;
                        if (qx * qx + qy * qy + qz * qz > radius2) continue;
                        ecs.MarkInactive(i);
                        affected = true;
                    }
                }
                (void)spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                        caster.auth_x, caster.auth_y - 0.6f, caster.auth_z,
                                        0.f, 0.f, 0.f, spell_ttl_ms(spell_id),
                                        PLAYER_SPELL_SHIELD_RADIUS, 0.f, 0u, now_ms);
                return affected || true;
            }
            case ge::item::Id::SpellMark: {
                io::i32 hit_x = 0, hit_y = 0, hit_z = 0;
                io::i32 prev_x = 0, prev_y = 0, prev_z = 0;
                if (!spell_raycast_first_solid(eye_x, eye_y, eye_z, dx, dy, dz, 18.0f, 0.2f,
                                               hit_x, hit_y, hit_z, prev_x, prev_y, prev_z)) {
                    prev_x = floor_to_i32(eye_x + dx * 8.0f);
                    prev_y = floor_to_i32(eye_y + dy * 8.0f);
                    prev_z = floor_to_i32(eye_z + dz * 8.0f);
                }
                return spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                         static_cast<float>(prev_x) + 0.5f,
                                         static_cast<float>(prev_y) + 0.25f,
                                         static_cast<float>(prev_z) + 0.5f,
                                         0.f, 0.f, 0.f,
                                         spell_ttl_ms(spell_id), 0.5f, 0.f, 0u, now_ms) >= 0;
            }
            case ge::item::Id::SpellPull: {
                bool affected = false;
                const float radius2 = PLAYER_SPELL_PULL_RADIUS * PLAYER_SPELL_PULL_RADIUS;
                if (world_actor_ecs) {
                    ActorEcs& ecs = *world_actor_ecs;
                    for (io::u32 i = 0u; i < WORLD_ACTOR_CAP; ++i) {
                        if (ecs.alive[i] == 0u || !ecs.net_sync[i].active) continue;
                        if (ecs.identity[i].model != ge::net::WORLD_ACTOR_MODEL_ITEM) continue;
                        const float qx = caster.auth_x - ecs.transform[i].x;
                        const float qy = (caster.auth_y - 0.95f) - ecs.transform[i].y;
                        const float qz = caster.auth_z - ecs.transform[i].z;
                        const float d2 = qx * qx + qy * qy + qz * qz;
                        if (d2 > radius2) continue;
                        const float inv = (d2 > 0.0001f) ? (1.f / (absf(qx) + absf(qy) + absf(qz))) : 0.f;
                        ecs.velocity[i].x = qx * inv * 8.0f;
                        ecs.velocity[i].y = qy * inv * 8.0f + 1.6f;
                        ecs.velocity[i].z = qz * inv * 8.0f;
                        ecs.net_sync[i].dirty = true;
                        affected = true;
                    }
                }
                (void)spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                        caster.auth_x, caster.auth_y - 0.55f, caster.auth_z,
                                        0.f, 0.f, 0.f, spell_ttl_ms(spell_id),
                                        PLAYER_SPELL_PULL_RADIUS, 0.f, 0u, now_ms);
                return affected || true;
            }
            case ge::item::Id::SpellBlinkStep: {
                float tx = caster.auth_x;
                float ty = caster.auth_y;
                float tz = caster.auth_z;
                bool moved = false;
                float step = PLAYER_SPELL_BLINK_STEP;
                for (io::u32 it = 0u; it < 5u; ++it) {
                    const float nx = caster.auth_x + dx * step;
                    const float ny = caster.auth_y + dy * step * 0.35f;
                    const float nz = caster.auth_z + dz * step;
                    float rx = nx, ry = ny, rz = nz;
                    if (caster.use_noclip || try_resolve_player_inside_solid(nx, ny, nz, rx, ry, rz, caster.crawling)) {
                        tx = rx; ty = ry; tz = rz;
                        moved = true;
                        break;
                    }
                    step *= 0.55f;
                }
                if (!moved) return false;
                caster.auth_x = tx;
                caster.auth_y = ty;
                caster.auth_z = tz;
                caster.pending_x = tx;
                caster.pending_y = ty;
                caster.pending_z = tz;
                caster.has_pending = false;
                caster.auth_ms = now_ms;
                caster.pending_ms = now_ms;
                sync_player_ecs_from_peer(peer_index);
                (void)send_pos(peer_index, now_ms, ge::net::PLAYER_POS_FLAG_CORRECTION, io::UdpChan::Reliable);
                broadcast_remote_pose_from_peer(peer_index, now_ms);
                (void)spawn_spell_actor(spell_id, static_cast<io::u8>(peer_index),
                                        tx, ty - 0.6f, tz, 0.f, 0.f, 0.f, spell_ttl_ms(spell_id), 0.55f, 0.f, 0u, now_ms);
                return true;
            }
            default:
                return false;
            }
        }

        inline void handle_melee_attack(io::u16 attacker_idx, const ge::net::MeleeAttackSample& sample, io::u64 now_ms) noexcept {
            if (!peers || attacker_idx >= static_cast<io::u16>(io::MAX_PEERS)) return;
            PeerState& attacker = peers[attacker_idx];
            if (!attacker.used || !attacker.has_auth || attacker.dead) {
                ++stats.melee_reject;
                return;
            }
            const io::u8 selected = (attacker.inventory.selected_hotbar < ge::item::HOTBAR_SLOT_COUNT)
                ? attacker.inventory.selected_hotbar : 0u;
            ge::item::Stack held = attacker.inventory.hotbar[selected];
            ge::item::normalize(held);

            if (held.id == ge::item::Id::SpellWard && !ge::item::is_empty(held)) {
                if (held.freshness == 0u || held.freshness == ge::item::FRESHNESS_MAX) {
                    ++stats.melee_reject;
                    return;
                }
                WardInstance* cfg = find_ward_instance(attacker, held.freshness);
                if (!cfg || !cfg->active || cfg->slots_available == 0u) {
                    ++stats.melee_reject;
                    return;
                }
                if (attacker.ward_cast_token != held.freshness) {
                    attacker.ward_cast_token = held.freshness;
                    attacker.ward_cast_cursor = 0u;
                    attacker.ward_next_cast_ms = 0u;
                    attacker.ward_reload_until_ms = 0u;
                }
                if (attacker.ward_next_cast_ms != 0u && now_ms < attacker.ward_next_cast_ms) {
                    ++stats.melee_reject;
                    return;
                }
                if (attacker.ward_reload_until_ms != 0u && now_ms < attacker.ward_reload_until_ms) {
                    ++stats.melee_reject;
                    return;
                }

                ge::item::Id spell_id = ge::item::Id::None;
                io::u8 spell_slot = 0u;
                bool found_spell = false;
                const io::u8 start = (attacker.ward_cast_cursor < cfg->slots_available) ? attacker.ward_cast_cursor : 0u;
                for (io::u8 n = 0u; n < cfg->slots_available; ++n) {
                    const io::u8 idx = static_cast<io::u8>((start + n) % cfg->slots_available);
                    ge::item::Stack s = cfg->spells[idx];
                    ge::item::normalize(s);
                    if (ge::item::is_empty(s)) continue;
                    if (!spell_is_supported(s.id)) continue;
                    spell_id = s.id;
                    spell_slot = idx;
                    found_spell = true;
                    break;
                }

                io::u32 cast_ms = static_cast<io::u32>(cfg->stat_delay_cast_x1000);
                if (cast_ms < PLAYER_SPELL_COOLDOWN_MIN_MS)
                    cast_ms = PLAYER_SPELL_COOLDOWN_MIN_MS;
                io::u32 reload_ms = static_cast<io::u32>(cfg->stat_delay_reload_x1000);
                if (reload_ms < 260u) reload_ms = 260u;

                if (!found_spell) {
                    attacker.ward_cast_cursor = 0u;
                    attacker.ward_reload_until_ms = now_ms + reload_ms;
                    ++stats.melee_reject;
                    return;
                }

                if (!cast_spell_from_ward(attacker_idx, spell_id, sample.yaw, sample.pitch, now_ms)) {
                    ++stats.melee_reject;
                    return;
                }

                attacker.ward_cast_cursor = static_cast<io::u8>(spell_slot + 1u);
                if (attacker.ward_cast_cursor >= cfg->slots_available) {
                    attacker.ward_cast_cursor = 0u;
                    attacker.ward_reload_until_ms = now_ms + reload_ms;
                }
                attacker.ward_next_cast_ms = now_ms + cast_ms + spell_extra_cast_ms(spell_id);
                region_note_ward_interaction(floor_to_i32(attacker.auth_x), floor_to_i32(attacker.auth_y), floor_to_i32(attacker.auth_z),
                                             true, now_ms);
                broadcast_remote_pose_from_peer(attacker_idx, now_ms);
                ++stats.melee_hit;
                return;
            }

            if (attacker.next_melee_ms != 0u && now_ms < attacker.next_melee_ms) {
                ++stats.melee_reject;
                return;
            }
            attacker.next_melee_ms = now_ms + PLAYER_MELEE_COOLDOWN_MS;

            const float reach2 = PLAYER_MELEE_REACH * PLAYER_MELEE_REACH;

            const float ax = attacker.auth_x;
            const float ay = attacker.auth_y - 0.10f;
            const float az = attacker.auth_z;

            io::i32 best_idx = -1;
            float best_d2 = reach2 + 1.f;
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                if (i == attacker_idx) continue;
                const PeerState& victim = peers[i];
                if (!victim.used || !victim.has_auth || victim.dead) continue;
                const float vx = victim.auth_x;
                const float vy = victim.auth_y - 0.85f;
                const float vz = victim.auth_z;
                const float dx = vx - ax;
                const float dy = vy - ay;
                const float dz = vz - az;
                const float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 > reach2) continue;
                if (d2 >= best_d2) continue;
                if (!melee_los_clear(ax, ay, az, vx, vy, vz)) continue;
                best_d2 = d2;
                best_idx = static_cast<io::i32>(i);
            }

            if (best_idx < 0) {
                ++stats.melee_reject;
                return;
            }

            PeerState& victim = peers[static_cast<io::u16>(best_idx)];
            if (victim.use_fly) {
                ++stats.melee_reject;
                return;
            }

            io::u16 damage = PLAYER_MELEE_DAMAGE_DEFAULT;
            if (held.id == ge::item::Id::RustyDagger && !ge::item::is_empty(held))
                damage = PLAYER_MELEE_DAMAGE_DAGGER;
            if (damage > victim.hp) damage = victim.hp;
            if (damage == 0u) {
                ++stats.melee_reject;
                return;
            }

            const float kx0 = victim.auth_x - attacker.auth_x;
            const float kz0 = victim.auth_z - attacker.auth_z;
            const float klen = absf(kx0) + absf(kz0);
            if (klen > 0.000001f) {
                const float inv_k = 1.f / klen;
                const float kx = kx0 * inv_k;
                const float kz = kz0 * inv_k;
                float nx = victim.auth_x + kx * PLAYER_MELEE_KNOCKBACK_XZ;
                float ny = victim.auth_y + PLAYER_MELEE_KNOCKBACK_Y;
                float nz = victim.auth_z + kz * PLAYER_MELEE_KNOCKBACK_XZ;
                float rx = nx, ry = ny, rz = nz;
                if (!victim.use_noclip && !try_resolve_player_inside_solid(nx, ny, nz, rx, ry, rz, victim.crawling)) {
                    rx = victim.auth_x;
                    ry = victim.auth_y;
                    rz = victim.auth_z;
                }
                victim.auth_x = rx;
                victim.auth_y = ry;
                victim.auth_z = rz;
                victim.pending_x = rx;
                victim.pending_y = ry;
                victim.pending_z = rz;
                victim.has_pending = false;
                victim.auth_ms = now_ms;
                victim.pending_ms = now_ms;
                sync_player_ecs_from_peer(static_cast<io::u16>(best_idx));
                (void)send_pos(static_cast<io::u16>(best_idx), now_ms, ge::net::PLAYER_POS_FLAG_CORRECTION, io::UdpChan::Reliable);
            }

            victim.hp = static_cast<io::u16>(victim.hp - damage);
            if (victim.hp == 0u) {
                kill_peer(static_cast<io::u16>(best_idx), victim, now_ms, static_cast<float>(damage), 0.f, ge::net::DeathReason::Unknown);
            } else {
                (void)send_health(static_cast<io::u16>(best_idx), now_ms, static_cast<float>(damage), 0.f, io::UdpChan::Reliable);
            }

            broadcast_remote_pose_from_peer(attacker_idx, now_ms);
            broadcast_remote_pose_from_peer(static_cast<io::u16>(best_idx), now_ms);
            ++stats.melee_hit;
        }

        inline bool send_health(io::u16 peer_index, io::u64 now_ms, float damage, float fall_blocks,
                                io::UdpChan chan, io::u8 extra_flags = 0u) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            PeerState& p = peers[peer_index];
            if (!p.used) return false;
            io::u16 hp = p.hp;
            if (player_ecs && player_ecs->alive[peer_index] != 0u)
                hp = player_ecs->health[peer_index].hp;
            ge::net::PlayerHealthSample sample{};
            sample.hp = hp;
            sample.damage = damage;
            sample.fall_blocks = fall_blocks;
            sample.hunger = p.hunger;
            sample.flags = (p.dead ? ge::net::PLAYER_HEALTH_FLAG_DEAD : 0u) | extra_flags;
            sample.death_reason = p.death_reason;
            ge::net::S2C_PlayerHealth wire{};
            ge::net::encode_s2c_player_health(sample, wire);
            const bool ok = loop.send_to_peer(p.ep, ge::net::PK_S2C_PLAYER_HEALTH, chan,
                                              io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            if (ok) {
                p.last_sent_hp = hp;
                p.last_sent_hunger = p.hunger;
                ++stats.send_ok;
            } else ++stats.send_fail;
            return ok;
        }

        inline bool send_world_time_to_peer(io::u16 peer_index, io::u64 now_ms, io::UdpChan chan) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            const PeerState& p = peers[peer_index];
            if (!p.used) return false;

            ge::net::WorldTimeSample sample{};
            sample.cycle_pos_ms = WorldPhaseMs(now_ms);
            sample.day_ms = world_time.day_ms;
            sample.night_ms = world_time.night_ms;
            ge::net::S2C_WorldTime wire{};
            ge::net::encode_s2c_world_time(sample, wire);

            const bool ok = loop.send_to_peer(
                p.ep,
                ge::net::PK_S2C_WORLD_TIME,
                chan,
                io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                now_ms);
            if (ok) ++stats.send_ok;
            else ++stats.send_fail;
            return ok;
        }

        IO_NODISCARD inline bool actor_has_line_of_sight(float ax, float ay, float az,
                                                         float bx, float by, float bz) const noexcept {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;
            float max_comp = absf(dx);
            const float ady = absf(dy);
            const float adz = absf(dz);
            if (ady > max_comp) max_comp = ady;
            if (adz > max_comp) max_comp = adz;
            if (max_comp <= 0.000001f) return true;
            io::u32 steps = io::to_u32(static_cast<double>(max_comp / WORLD_MOB_LOS_STEP));
            if (steps < 1u) steps = 1u;
            if (steps > 128u) steps = 128u;
            for (io::u32 i = 1u; i < steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float sx = ax + dx * t;
                const float sy = ay + dy * t;
                const float sz = az + dz * t;
                const ge::voxel::BlockId id = world.get_world_block(floor_to_i32(sx), floor_to_i32(sy), floor_to_i32(sz));
                if (ge::voxel::is_solid(id) && !is_book_anchor(id))
                    return false;
            }
            return true;
        }

        IO_NODISCARD inline bool actor_to_sample(io::u32 actor_index, ge::net::WorldActorSample& sample) const noexcept {
            if (!world_actor_ecs) return false;
            if (actor_index >= WORLD_ACTOR_CAP) return false;
            const ActorEcs& ecs = *world_actor_ecs;
            if (ecs.alive[actor_index] == 0u) return false;
            if (ecs.identity[actor_index].actor_id == 0u) return false;

            sample = {};
            sample.actor_id = ecs.identity[actor_index].actor_id;
            sample.model = ecs.identity[actor_index].model;
            sample.mode = ecs.identity[actor_index].mode;
            sample.state = ecs.mob_state[actor_index].net_state;
            sample.anim = ecs.mob_state[actor_index].net_anim;
            sample.flags = ecs.net_sync[actor_index].active ? ge::net::WORLD_ACTOR_FLAG_ACTIVE : 0u;
            if (ecs.identity[actor_index].model == ge::net::WORLD_ACTOR_MODEL_ITEM && ecs.item_drop[actor_index].grounded)
                sample.flags |= ge::net::WORLD_ACTOR_FLAG_GROUNDED;
            sample.x = ecs.transform[actor_index].x;
            sample.y = ecs.transform[actor_index].y;
            sample.z = ecs.transform[actor_index].z;
            return true;
        }

        inline void broadcast_world_actor_state(io::u32 actor_index, io::u64 now_ms, io::UdpChan chan) noexcept {
            if (!peers) return;
            ge::net::WorldActorSample sample{};
            if (!actor_to_sample(actor_index, sample)) return;
            ge::net::S2C_WorldActor wire{};
            ge::net::encode_s2c_world_actor(sample, wire);

            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used) continue;
                const bool ok = loop.send_to_peer(
                    p.ep,
                    ge::net::PK_S2C_WORLD_ACTOR,
                    chan,
                    io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                    now_ms);
                if (ok) ++stats.send_ok;
                else ++stats.send_fail;
            }
        }

        inline void broadcast_world_actor_snapshot_to_peer(io::u16 peer_index, io::u64 now_ms) noexcept {
            if (!peers || !world_actor_ecs) return;
            if (peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            const PeerState& p = peers[peer_index];
            if (!p.used) return;

            for (io::u16 i = 0; i < WORLD_ACTOR_CAP; ++i) {
                ge::net::WorldActorSample sample{};
                if (!actor_to_sample(i, sample)) continue;
                ge::net::S2C_WorldActor wire{};
                ge::net::encode_s2c_world_actor(sample, wire);
                const bool ok = loop.send_to_peer(
                    p.ep,
                    ge::net::PK_S2C_WORLD_ACTOR,
                    io::UdpChan::Reliable,
                    io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                    now_ms);
                if (ok) ++stats.send_ok;
                else ++stats.send_fail;
            }
        }

        inline void refresh_item_actor_visual_count(io::u32 actor_index) noexcept {
            if (!world_actor_ecs || actor_index >= WORLD_ACTOR_CAP) return;
            ActorEcs& ecs = *world_actor_ecs;
            if (ecs.alive[actor_index] == 0u) return;
            ge::item::Stack& stack = ecs.item_drop[actor_index].stack;
            io::u32 pile = ecs.item_drop[actor_index].pile_count;
            if (pile == 0u) pile = stack.count;
            if (pile == 0u) {
                ge::item::clear(stack);
                ecs.item_drop[actor_index].pile_count = 0u;
                return;
            }
            io::u16 vis = static_cast<io::u16>(pile > 65535u ? 65535u : pile);
            const io::u16 limit = ge::item::max_stack(stack.id);
            if (vis > limit) vis = limit;
            if (vis == 0u) vis = 1u;
            stack.count = vis;
            ge::item::normalize(stack);
            ecs.item_drop[actor_index].pile_count = pile;
        }

        inline void merge_item_actors(io::u32 dst_index, io::u32 src_index) noexcept {
            if (!world_actor_ecs) return;
            if (dst_index >= WORLD_ACTOR_CAP || src_index >= WORLD_ACTOR_CAP || dst_index == src_index) return;
            ActorEcs& ecs = *world_actor_ecs;
            if (ecs.alive[dst_index] == 0u || ecs.alive[src_index] == 0u) return;
            if (!ecs.net_sync[dst_index].active || !ecs.net_sync[src_index].active) return;
            if (ecs.identity[dst_index].model != ge::net::WORLD_ACTOR_MODEL_ITEM) return;
            if (ecs.identity[src_index].model != ge::net::WORLD_ACTOR_MODEL_ITEM) return;
            ge::item::Stack& dst = ecs.item_drop[dst_index].stack;
            ge::item::Stack& src = ecs.item_drop[src_index].stack;
            ge::item::normalize(dst);
            ge::item::normalize(src);
            if (ge::item::is_empty(dst) || ge::item::is_empty(src)) return;
            if (!ge::item::can_stack_together(dst, src)) return;

            io::u32 dst_count = ecs.item_drop[dst_index].pile_count;
            io::u32 src_count = ecs.item_drop[src_index].pile_count;
            if (dst_count == 0u) dst_count = dst.count;
            if (src_count == 0u) src_count = src.count;
            if (src_count == 0u) return;
            if (dst_count > 0xFFFFFFFFu - src_count)
                dst_count = 0xFFFFFFFFu;
            else
                dst_count += src_count;

            ecs.item_drop[dst_index].pile_count = dst_count;
            if (ge::item::decays(dst.id) && src.freshness < dst.freshness)
                dst.freshness = src.freshness;
            refresh_item_actor_visual_count(dst_index);
            ecs.net_sync[dst_index].dirty = true;
            despawn_item_actor(src_index);
        }

        inline void update_world_actors(io::u32 dt_ms, io::u64 now_ms) noexcept {
            if (!world_actor_ecs) return;
            ActorEcs& ecs = *world_actor_ecs;
            const float dt_sec = static_cast<float>(dt_ms) * 0.001f;
            const float see_radius2 = WORLD_MOB_SEE_RADIUS * WORLD_MOB_SEE_RADIUS;
            const float item_merge_radius2 = WORLD_ITEM_MERGE_RADIUS * WORLD_ITEM_MERGE_RADIUS;

            for (io::u16 i = 0; i < WORLD_ACTOR_CAP; ++i) {
                if (ecs.alive[i] == 0u) continue;
                if (!ecs.net_sync[i].active) {
                    if (ecs.net_sync[i].dirty) {
                        broadcast_world_actor_state(i, now_ms, io::UdpChan::Reliable);
                        ecs.net_sync[i].dirty = false;
                        ecs.net_sync[i].last_broadcast_ms = now_ms;
                        ecs.Erase(i);
                    }
                    continue;
                }

                if (ecs.identity[i].model == ge::net::WORLD_ACTOR_MODEL_SPELL) {
                    ge::item::Id sid = ecs.spell_fx[i].spell;
                    if (!spell_is_supported(sid))
                        sid = static_cast<ge::item::Id>(ecs.mob_state[i].net_state);
                    if (!spell_is_supported(sid)) {
                        ecs.MarkInactive(i);
                        continue;
                    }
                    ecs.spell_fx[i].spell = sid;
                    ecs.mob_state[i].net_state = static_cast<io::u8>(sid);
                    if (ecs.spell_fx[i].ttl_ms <= dt_ms) {
                        ecs.MarkInactive(i);
                        continue;
                    }
                    ecs.spell_fx[i].ttl_ms -= dt_ms;

                    if (sid == ge::item::Id::SpellMine) {
                        if (ecs.spell_fx[i].power > 0.f) {
                            const float remaining = ecs.spell_fx[i].power - static_cast<float>(dt_ms);
                            ecs.spell_fx[i].power = (remaining > 0.f) ? remaining : 0.f;
                            if (ecs.spell_fx[i].power <= 0.f)
                                ecs.spell_fx[i].flags |= 0x01u; // armed
                        } else {
                            ecs.spell_fx[i].flags |= 0x01u;
                        }

                        if ((ecs.spell_fx[i].flags & 0x01u) != 0u && peers) {
                            const io::u8 owner = ecs.spell_fx[i].owner_peer;
                            const float trigger = (ecs.spell_fx[i].radius > 0.15f)
                                ? ecs.spell_fx[i].radius
                                : PLAYER_SPELL_MINE_TRIGGER_RADIUS;
                            const float trigger2 = trigger * trigger;
                            bool detonated = false;
                            for (io::u16 pidx = 0u; pidx < static_cast<io::u16>(io::MAX_PEERS); ++pidx) {
                                if (pidx == static_cast<io::u16>(owner)) continue;
                                const PeerState& p = peers[pidx];
                                if (!p.used || !p.has_auth || p.dead) continue;
                                const float dx = p.auth_x - ecs.transform[i].x;
                                const float dy = (p.auth_y - 0.85f) - ecs.transform[i].y;
                                const float dz = p.auth_z - ecs.transform[i].z;
                                if (dx * dx + dy * dy + dz * dz > trigger2) continue;
                                const io::u16 dmg = (spell_base_damage(sid) > 0u) ? spell_base_damage(sid) : 10u;
                                (void)apply_spell_damage_to_peer(static_cast<io::u16>(owner), pidx, dmg, 1.9f, 0.22f, now_ms);
                                detonated = true;
                            }
                            if (detonated) {
                                ecs.MarkInactive(i);
                                continue;
                            }
                        }
                    } else if (sid == ge::item::Id::SpellMark ||
                               sid == ge::item::Id::SpellShieldPulse ||
                               sid == ge::item::Id::SpellPull ||
                               sid == ge::item::Id::SpellBlinkStep ||
                               sid == ge::item::Id::SpellBeam) {
                        // Pure visual/short-lived world markers: no movement step.
                    } else {
                        // Projectile-like spell entities.
                        ecs.transform[i].x += ecs.velocity[i].x * dt_sec;
                        ecs.transform[i].y += ecs.velocity[i].y * dt_sec;
                        ecs.transform[i].z += ecs.velocity[i].z * dt_sec;

                        const ge::voxel::BlockId bid = world.get_world_block(
                            floor_to_i32(ecs.transform[i].x),
                            floor_to_i32(ecs.transform[i].y),
                            floor_to_i32(ecs.transform[i].z));
                        if (bid != ge::voxel::BlockId::Air && !ge::voxel::is_liquid(bid)) {
                            if (sid == ge::item::Id::SpellOrb && peers) {
                                const io::u8 owner = ecs.spell_fx[i].owner_peer;
                                const float r = 2.2f;
                                const float r2 = r * r;
                                for (io::u16 pidx = 0u; pidx < static_cast<io::u16>(io::MAX_PEERS); ++pidx) {
                                    if (pidx == static_cast<io::u16>(owner)) continue;
                                    const PeerState& p = peers[pidx];
                                    if (!p.used || !p.has_auth || p.dead) continue;
                                    const float dx = p.auth_x - ecs.transform[i].x;
                                    const float dy = (p.auth_y - 0.85f) - ecs.transform[i].y;
                                    const float dz = p.auth_z - ecs.transform[i].z;
                                    if (dx * dx + dy * dy + dz * dz > r2) continue;
                                    (void)apply_spell_damage_to_peer(static_cast<io::u16>(owner), pidx, spell_base_damage(sid), 1.1f, 0.16f, now_ms);
                                }
                            }
                            ecs.MarkInactive(i);
                            continue;
                        }

                        if (peers) {
                            const io::u8 owner = ecs.spell_fx[i].owner_peer;
                            const float hit_r = (sid == ge::item::Id::SpellOrb) ? 0.6f : 0.38f;
                            const float hit_r2 = hit_r * hit_r;
                            bool hit_peer = false;
                            for (io::u16 pidx = 0u; pidx < static_cast<io::u16>(io::MAX_PEERS); ++pidx) {
                                if (pidx == static_cast<io::u16>(owner)) continue;
                                const PeerState& p = peers[pidx];
                                if (!p.used || !p.has_auth || p.dead) continue;
                                const float dx = p.auth_x - ecs.transform[i].x;
                                const float dy = (p.auth_y - 0.85f) - ecs.transform[i].y;
                                const float dz = p.auth_z - ecs.transform[i].z;
                                if (dx * dx + dy * dy + dz * dz > hit_r2) continue;
                                (void)apply_spell_damage_to_peer(static_cast<io::u16>(owner), pidx, spell_base_damage(sid), 0.9f, 0.12f, now_ms);
                                hit_peer = true;
                                if (sid == ge::item::Id::SpellOrb) {
                                    const float r = 1.85f;
                                    const float r2 = r * r;
                                    for (io::u16 nidx = 0u; nidx < static_cast<io::u16>(io::MAX_PEERS); ++nidx) {
                                        if (nidx == static_cast<io::u16>(owner) || nidx == pidx) continue;
                                        const PeerState& np = peers[nidx];
                                        if (!np.used || !np.has_auth || np.dead) continue;
                                        const float qx = np.auth_x - ecs.transform[i].x;
                                        const float qy = (np.auth_y - 0.85f) - ecs.transform[i].y;
                                        const float qz = np.auth_z - ecs.transform[i].z;
                                        if (qx * qx + qy * qy + qz * qz > r2) continue;
                                        (void)apply_spell_damage_to_peer(static_cast<io::u16>(owner), nidx,
                                                                         static_cast<io::u16>(spell_base_damage(sid) / 2u),
                                                                         0.55f, 0.08f, now_ms);
                                    }
                                }
                                break;
                            }
                            if (hit_peer) {
                                ecs.MarkInactive(i);
                                continue;
                            }
                        }
                    }
                    ecs.net_sync[i].dirty = true;
                } else if (ecs.identity[i].model == ge::net::WORLD_ACTOR_MODEL_ITEM) {
                    ge::voxel::ChunkCoord item_cc{};
                    io::u32 ilx = 0u, ily = 0u, ilz = 0u;
                    ge::voxel::split_world_coord(
                        floor_to_i32(ecs.transform[i].x),
                        floor_to_i32(ecs.transform[i].y),
                        floor_to_i32(ecs.transform[i].z),
                        item_cc, ilx, ily, ilz);
                    const bool chunk_hot = is_chunk_hot_any(item_cc);
                    if (!chunk_hot)
                        continue;
                    if (ecs.item_drop[i].despawn_ms == 0u)
                        ecs.item_drop[i].despawn_ms = WORLD_ITEM_DESPAWN_MS;
                    if (ecs.item_drop[i].despawn_ms <= dt_ms) {
                        despawn_item_actor(i);
                        continue;
                    }
                    ecs.item_drop[i].despawn_ms -= dt_ms;
                    ge::item::age_stack(ecs.item_drop[i].stack, dt_ms);
                    if (ge::item::is_empty(ecs.item_drop[i].stack)) {
                        despawn_item_actor(i);
                        continue;
                    }
                    if (ecs.item_drop[i].pile_count == 0u)
                        ecs.item_drop[i].pile_count = ecs.item_drop[i].stack.count;
                    refresh_item_actor_visual_count(i);
                    ecs.mob_state[i].net_state = static_cast<io::u8>(ecs.item_drop[i].stack.id);
                    ecs.mob_state[i].net_anim = static_cast<io::u8>(ge::item::freshness_band(ecs.item_drop[i].stack));

                    for (io::u16 j = static_cast<io::u16>(i + 1u); j < WORLD_ACTOR_CAP; ++j) {
                        if (ecs.alive[j] == 0u) continue;
                        if (!ecs.net_sync[j].active) continue;
                        if (ecs.identity[j].model != ge::net::WORLD_ACTOR_MODEL_ITEM) continue;
                        ge::item::Stack& other = ecs.item_drop[j].stack;
                        ge::item::normalize(other);
                        if (ge::item::is_empty(other)) continue;
                        if (!ge::item::can_stack_together(ecs.item_drop[i].stack, other)) continue;
                        const float dxm = ecs.transform[j].x - ecs.transform[i].x;
                        const float dym = ecs.transform[j].y - ecs.transform[i].y;
                        const float dzm = ecs.transform[j].z - ecs.transform[i].z;
                        if (dxm * dxm + dym * dym + dzm * dzm > item_merge_radius2) continue;
                        merge_item_actors(i, j);
                    }

                    io::i32 pickup_peer = -1;
                    float best_d2 = WORLD_ITEM_PICKUP_RADIUS * WORLD_ITEM_PICKUP_RADIUS;
                    if (peers) {
                        for (io::u16 pidx = 0; pidx < static_cast<io::u16>(io::MAX_PEERS); ++pidx) {
                            const PeerState& p = peers[pidx];
                            if (!p.used || !p.has_auth || p.dead) continue;
                            const float dx = p.auth_x - ecs.transform[i].x;
                            const float dy = (p.auth_y - 1.0f) - ecs.transform[i].y;
                            const float dz = p.auth_z - ecs.transform[i].z;
                            const float d2 = dx * dx + dy * dy + dz * dz;
                            if (d2 <= best_d2) {
                                best_d2 = d2;
                                pickup_peer = static_cast<io::i32>(pidx);
                            }
                        }
                    }

                    if (pickup_peer >= 0) {
                        io::u32 pile_left = ecs.item_drop[i].pile_count;
                        if (pile_left == 0u) pile_left = ecs.item_drop[i].stack.count;
                        bool picked_any = false;
                        while (pile_left > 0u) {
                            ge::item::Stack give = ecs.item_drop[i].stack;
                            io::u16 give_count = static_cast<io::u16>(pile_left > 65535u ? 65535u : pile_left);
                            const io::u16 max_count = ge::item::max_stack(give.id);
                            if (give_count > max_count) give_count = max_count;
                            give.count = give_count;
                            if (!give_item_to_peer(static_cast<io::u16>(pickup_peer), give, now_ms, false))
                                break;
                            pile_left -= give_count;
                            picked_any = true;
                        }
                        if (picked_any) {
                            (void)send_inventory_to_peer(static_cast<io::u16>(pickup_peer), now_ms, io::UdpChan::Reliable);
                            if (pile_left == 0u) {
                                despawn_item_actor(i);
                            } else {
                                ecs.item_drop[i].pile_count = pile_left;
                                refresh_item_actor_visual_count(i);
                                ecs.net_sync[i].dirty = true;
                            }
                        }
                    } else {
                        ecs.velocity[i].y -= WORLD_ITEM_GRAVITY * dt_sec;
                        if (ecs.velocity[i].y < -WORLD_ITEM_TERMINAL_SPEED)
                            ecs.velocity[i].y = -WORLD_ITEM_TERMINAL_SPEED;
                        ecs.transform[i].x += ecs.velocity[i].x * dt_sec;
                        ecs.transform[i].y += ecs.velocity[i].y * dt_sec;
                        ecs.transform[i].z += ecs.velocity[i].z * dt_sec;

                        const io::i32 foot_x = floor_to_i32(ecs.transform[i].x);
                        const io::i32 foot_y = floor_to_i32(ecs.transform[i].y - 0.35f);
                        const io::i32 foot_z = floor_to_i32(ecs.transform[i].z);
                        if (is_solid_world_cell_for_ground(foot_x, foot_y, foot_z)) {
                            ecs.transform[i].y = static_cast<float>(foot_y + 1) + 0.08f;
                            ecs.velocity[i] = {};
                            ecs.item_drop[i].grounded = true;
                        } else {
                            ecs.item_drop[i].grounded = false;
                        }
                        ecs.net_sync[i].dirty = true;
                    }
                } else if (ecs.identity[i].mode == ge::net::WORLD_ACTOR_MODE_ENTITY) {
                    if (ecs.mob_state[i].net_state != ge::net::WORLD_ACTOR_STATE_ENTITY_STAY ||
                        ecs.mob_state[i].net_anim != ge::net::WORLD_ACTOR_ANIM_STAY) {
                        ecs.mob_state[i].logical = ge::ecs::MobLogicState::Idle;
                        ecs.mob_state[i].net_state = ge::net::WORLD_ACTOR_STATE_ENTITY_STAY;
                        ecs.mob_state[i].net_anim = ge::net::WORLD_ACTOR_ANIM_STAY;
                        ecs.net_sync[i].dirty = true;
                    }
                } else {
                    io::i32 best_peer = -1;
                    float best_d2 = see_radius2 + 1.f;
                    for (io::u16 pidx = 0; pidx < static_cast<io::u16>(io::MAX_PEERS); ++pidx) {
                        if (!peers) break;
                        const PeerState& p = peers[pidx];
                        if (!p.used || !p.has_auth) continue;
                        const float dx = p.auth_x - ecs.transform[i].x;
                        const float dy = (p.auth_y - 0.6f) - ecs.transform[i].y;
                        const float dz = p.auth_z - ecs.transform[i].z;
                        const float d2 = dx * dx + dy * dy + dz * dz;
                        if (d2 > see_radius2) continue;
                        if (d2 < best_d2) {
                            best_d2 = d2;
                            best_peer = static_cast<io::i32>(pidx);
                        }
                    }

                    bool sees_player = false;
                    float tx = ecs.transform[i].x, ty = ecs.transform[i].y, tz = ecs.transform[i].z;
                    if (best_peer >= 0 && peers) {
                        const PeerState& p = peers[static_cast<io::u16>(best_peer)];
                        tx = p.auth_x;
                        ty = p.auth_y - 0.5f;
                        tz = p.auth_z;
                        sees_player = actor_has_line_of_sight(ecs.transform[i].x, ecs.transform[i].y, ecs.transform[i].z, tx, ty, tz);
                    }

                    if (sees_player) {
                        const io::u8 prev_state = ecs.mob_state[i].net_state;
                        const io::u8 prev_anim = ecs.mob_state[i].net_anim;
                        ecs.mob_state[i].logical = ge::ecs::MobLogicState::Run;
                        ecs.mob_state[i].net_state = ge::net::WORLD_ACTOR_STATE_MOB_CHASE;
                        ecs.mob_state[i].net_anim = ge::net::WORLD_ACTOR_ANIM_LEVITATE;
                        ecs.mob_sim[i].target_peer = best_peer;

                        const float dx = tx - ecs.transform[i].x;
                        const float dy = ty - ecs.transform[i].y;
                        const float dz = tz - ecs.transform[i].z;
                        float max_comp = absf(dx);
                        const float ady = absf(dy);
                        const float adz = absf(dz);
                        if (ady > max_comp) max_comp = ady;
                        if (adz > max_comp) max_comp = adz;
                        if (max_comp > 0.000001f) {
                            const float step = WORLD_MOB_SPEED * dt_sec;
                            const float inv_len = 1.f / max_comp;
                            const float nx = dx * inv_len;
                            const float ny = dy * inv_len;
                            const float nz = dz * inv_len;
                            ecs.mob_sim[i].dir_x = nx;
                            ecs.mob_sim[i].dir_y = ny;
                            ecs.mob_sim[i].dir_z = nz;
                            ecs.velocity[i].x = nx * WORLD_MOB_SPEED;
                            ecs.velocity[i].y = ny * WORLD_MOB_SPEED;
                            ecs.velocity[i].z = nz * WORLD_MOB_SPEED;
                            ecs.transform[i].x += nx * step;
                            ecs.transform[i].y += ny * step;
                            ecs.transform[i].z += nz * step;
                            ecs.net_sync[i].dirty = true;
                        }
                        if (ecs.mob_state[i].net_state != prev_state || ecs.mob_state[i].net_anim != prev_anim)
                            ecs.net_sync[i].dirty = true;
                    } else {
                        const io::u8 prev_state = ecs.mob_state[i].net_state;
                        const io::u8 prev_anim = ecs.mob_state[i].net_anim;
                        ecs.mob_sim[i].target_peer = -1;
                        ecs.mob_state[i].logical = ge::ecs::MobLogicState::Idle;
                        ecs.mob_state[i].net_state = ge::net::WORLD_ACTOR_STATE_MOB_IDLE;
                        ecs.mob_state[i].net_anim = ge::net::WORLD_ACTOR_ANIM_STAY;
                        ecs.velocity[i] = {};
                        if (ecs.mob_state[i].net_state != prev_state || ecs.mob_state[i].net_anim != prev_anim)
                            ecs.net_sync[i].dirty = true;
                    }
                }

                const bool periodic = (now_ms - ecs.net_sync[i].last_broadcast_ms) >= WORLD_ACTOR_REBROADCAST_MS;
                if (ecs.net_sync[i].dirty || periodic) {
                    broadcast_world_actor_state(i, now_ms, io::UdpChan::Unreliable);
                    ecs.net_sync[i].dirty = false;
                    ecs.net_sync[i].last_broadcast_ms = now_ms;
                }
            }
        }

        inline void update_peer_hunger(io::u16 peer_index, PeerState& p, io::u64 now_ms,
                                       float dx, float dz) noexcept {
            if (p.dead || !p.used) return;

            const float horizontal_distance = absf(dx) + absf(dz);
            p.hunger_move_accum += horizontal_distance;

            if (p.hunger_move_accum >= PLAYER_HUNGER_MOVE_DISTANCE) {
                io::u32 steps = io::to_u32(p.hunger_move_accum / PLAYER_HUNGER_MOVE_DISTANCE);
                if (steps > 8u) steps = 8u;
                p.hunger_move_accum -= static_cast<float>(steps) * PLAYER_HUNGER_MOVE_DISTANCE;
                io::u32 hunger_now = p.hunger;
                hunger_now = (hunger_now > steps) ? (hunger_now - steps) : 0u;
                p.hunger = static_cast<io::u8>(hunger_now);
            }

            if (p.next_hunger_tick_ms == 0u)
                p.next_hunger_tick_ms = now_ms + PLAYER_HUNGER_IDLE_TICK_MS;
            if (now_ms >= p.next_hunger_tick_ms) {
                if (p.hunger > 0u) --p.hunger;
                p.next_hunger_tick_ms = now_ms + PLAYER_HUNGER_IDLE_TICK_MS;
            }

            if (p.hunger == 0u) {
                if (p.next_starve_tick_ms == 0u)
                    p.next_starve_tick_ms = now_ms + PLAYER_HUNGER_STARVE_TICK_MS;
                if (now_ms >= p.next_starve_tick_ms && p.hp > 0u) {
                    const io::u16 damage = 1u;
                    p.hp = static_cast<io::u16>(p.hp - damage);
                    if (p.hp == 0u) {
                        kill_peer(peer_index, p, now_ms, static_cast<float>(damage), 0.f, ge::net::DeathReason::Starvation);
                        return;
                    }
                    (void)send_health(peer_index, now_ms, static_cast<float>(damage), 0.f, io::UdpChan::Reliable);
                    p.next_starve_tick_ms = now_ms + PLAYER_HUNGER_STARVE_TICK_MS;
                }
            } else {
                p.next_starve_tick_ms = 0u;
            }

            if (p.hunger != p.last_sent_hunger)
                (void)send_health(peer_index, now_ms, 0.f, 0.f, io::UdpChan::Unreliable);
        }

        inline void update_peer_fall_damage(io::u16 peer_index, PeerState& p, io::u32 dt_ms,
                                            io::u64 now_ms, float dx, float dy, float dz) noexcept {
            if (p.use_fly || p.use_noclip || !p.has_auth || p.dead) {
                p.grounded = false;
                p.airborne = false;
                p.airborne_ms = 0u;
                return;
            }
            update_peer_hunger(peer_index, p, now_ms, dx, dz);
            if (p.dead || !p.has_auth) return;

            const bool grounded_now = is_player_grounded(p.auth_x, p.auth_y, p.auth_z, p.crawling);
            const float feet_y = p.auth_y - player_eye_to_feet_for_mode(p.crawling);
            if (!grounded_now) {
                if (!p.airborne) {
                    p.airborne = true;
                    p.air_peak_foot_y = feet_y;
                    p.airborne_ms = 0u;
                } else if (feet_y > p.air_peak_foot_y) {
                    p.air_peak_foot_y = feet_y;
                }
                p.airborne_ms += dt_ms;
            } else {
                if (p.airborne) {
                    float fall_blocks = p.air_peak_foot_y - feet_y;
                    if (fall_blocks < 0.f) fall_blocks = 0.f;
                    if (fall_blocks > PLAYER_FALL_DAMAGE_START_BLOCKS) {
                        const float damage_f = (fall_blocks - PLAYER_FALL_DAMAGE_START_BLOCKS) * PLAYER_FALL_DAMAGE_PER_BLOCK;
                        io::u16 damage = static_cast<io::u16>(io::to_u32(damage_f + 0.5f));
                        if (damage > p.hp) damage = p.hp;
                        p.hp = static_cast<io::u16>(p.hp - damage);
                        if (p.hp == 0u) {
                            kill_peer(peer_index, p, now_ms, static_cast<float>(damage), fall_blocks, ge::net::DeathReason::Fall);
                            return;
                        }
                        (void)send_health(peer_index, now_ms, static_cast<float>(damage), fall_blocks, io::UdpChan::Reliable);
                    }
                }
                p.airborne = false;
                p.airborne_ms = 0u;
            }
            p.grounded = grounded_now;
        }

        inline void sample_and_broadcast_tps(io::u64 now_ms) noexcept {
            if (!peers) return;
            const io::u32 now_rel_ms = static_cast<io::u32>(now_ms - boot_ms);
            if (stats.tps_window_start_ms == 0u)
                stats.tps_window_start_ms = now_rel_ms;

            const io::u32 elapsed_ms = now_rel_ms - stats.tps_window_start_ms;
            if (elapsed_ms < SERVER_TPS_BROADCAST_INTERVAL_MS) return;

            io::u16 tps_x100 = 0u;
            if (stats.tps_tick_count > 0u && elapsed_ms > 0u) {
                const io::u32 scaled = stats.tps_tick_count * 100000u;
                io::u32 val = scaled / elapsed_ms;
                const io::u32 max_tps_x100 = 100000u / SERVER_SIM_TICK_MS;
                if (val > max_tps_x100) val = max_tps_x100;
                if (val > 65535u) val = 65535u;
                tps_x100 = static_cast<io::u16>(val);
            }

            stats.tps_window_start_ms = now_rel_ms;
            stats.tps_tick_count = 0u;

            ge::net::ServerTpsSample sample{};
            sample.tps_x100 = tps_x100;
            ge::net::S2C_ServerTps wire{};
            ge::net::encode_s2c_server_tps(sample, wire);
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used) continue;
                const bool ok = loop.send_to_peer(p.ep, ge::net::PK_S2C_SERVER_TPS, io::UdpChan::Unreliable,
                                                  io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
                if (ok) ++stats.send_ok;
                else ++stats.send_fail;
            }
        }

        inline void sample_and_broadcast_world_time(io::u64 now_ms) noexcept {
            if (!peers) return;
            if (world_time.next_broadcast_ms != 0u && now_ms < world_time.next_broadcast_ms) return;
            world_time.next_broadcast_ms = now_ms + SERVER_WORLD_TIME_BROADCAST_INTERVAL_MS;

            ge::net::WorldTimeSample sample{};
            sample.cycle_pos_ms = WorldPhaseMs(now_ms);
            sample.day_ms = world_time.day_ms;
            sample.night_ms = world_time.night_ms;
            ge::net::S2C_WorldTime wire{};
            ge::net::encode_s2c_world_time(sample, wire);

            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used) continue;
                const bool ok = loop.send_to_peer(
                    p.ep,
                    ge::net::PK_S2C_WORLD_TIME,
                    io::UdpChan::Unreliable,
                    io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                    now_ms);
                if (ok) ++stats.send_ok;
                else ++stats.send_fail;
            }
        }

        IO_NODISCARD inline ge::voxel::ChunkCoord player_chunk(const PeerState& p) const noexcept {
            const io::i32 wx = floor_to_i32(p.auth_x);
            const io::i32 wy = floor_to_i32(p.auth_y);
            const io::i32 wz = floor_to_i32(p.auth_z);
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0, ly = 0, lz = 0;
            ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            return cc;
        }

        inline void force_peer_spawn(io::u16 peer_index, PeerState& p, io::u64 now_ms) noexcept {
            float sx = PLAYER_SPAWN_X;
            float sy = PLAYER_SPAWN_Y;
            float sz = PLAYER_SPAWN_Z;
            if (!try_resolve_player_inside_solid(sx, sy, sz, sx, sy, sz, false)) {
                sx = PLAYER_SPAWN_X;
                sy = PLAYER_SPAWN_Y;
                sz = PLAYER_SPAWN_Z;
            }
            p.has_auth = true;
            p.auth_x = sx;
            p.auth_y = sy;
            p.auth_z = sz;
            p.pending_x = sx;
            p.pending_y = sy;
            p.pending_z = sz;
            p.pending_ms = now_ms;
            p.auth_ms = now_ms;
            p.move_dir_x = 0;
            p.move_dir_y = 0;
            p.move_dir_z = 0;
            p.crawling = false;
            p.crawl_transition_state = 0u;
            p.crawl_transition_until_ms = 0u;
            p.action_flags = 0u;
            p.anim_state = ge::net::PLAYER_ANIM_STILL;
            p.grounded = is_player_grounded(sx, sy, sz, p.crawling);
            p.airborne = false;
            p.airborne_ms = 0u;
            p.air_peak_foot_y = sy - player_eye_to_feet_for_mode(p.crawling);
            p.stream_center_valid = false;
            p.has_pending = false;
            p.dead = false;
            p.death_reason = ge::net::DeathReason::None;
            p.respawn_at_ms = 0u;
            stream_reset(peer_index);
            (void)send_pos(peer_index, now_ms, ge::net::PLAYER_POS_FLAG_CORRECTION, io::UdpChan::Reliable);
        }

        inline void kill_peer(io::u16 peer_index, PeerState& p, io::u64 now_ms, float damage, float fall_blocks,
                              ge::net::DeathReason reason) noexcept {
            if (p.has_auth) {
                region_apply_delta_at_world(floor_to_i32(p.auth_x), floor_to_i32(p.auth_y), floor_to_i32(p.auth_z),
                                            -1400, 260, 3200, now_ms);
                region_save_needed = true;
            }
            p.dead = false;
            p.death_reason = ge::net::DeathReason::None;
            p.respawn_at_ms = 0u;
            p.hp = PLAYER_HP_MAX;
            p.hunger = PLAYER_HUNGER_MAX;
            p.hunger_move_accum = 0.f;
            p.next_hunger_tick_ms = now_ms + PLAYER_HUNGER_IDLE_TICK_MS;
            p.next_starve_tick_ms = 0u;
            p.next_vitals_sync_ms = now_ms + PLAYER_VITALS_SYNC_INTERVAL_MS;
            p.last_sent_hp = PLAYER_HP_MAX;
            p.last_sent_hunger = PLAYER_HUNGER_MAX;
            p.use_fly = false;
            p.use_noclip = false;
            p.chunk_ack_pending = false;
            p.chunk_ack_request_id = 0u;
            p.chunk_ack_sent_ms = 0u;
            cleanup_peer(peer_index);
            force_peer_spawn(peer_index, p, now_ms);
            (void)ensure_respawn_dagger(p);
            sync_player_ecs_from_peer(peer_index);
            sync_inventory_if_needed(peer_index, now_ms, true);
            (void)send_health(peer_index, now_ms, damage, fall_blocks, io::UdpChan::Reliable,
                              ge::net::PLAYER_HEALTH_FLAG_RESPAWN_RESET);
            io::StackOut<128> msg{};
            msg << "You died: " << ge::net::DeathReasonStr(reason);
            send_system_chat_to_peer(peer_index, msg.view(), now_ms);

            io::char_view dead_name{};
            io::StackOut<32> fallback_name{};
            if (p.name_len > 0u) dead_name = io::char_view{ p.name_utf8, p.name_len };
            else dead_name = peer_fallback_name(peer_index, fallback_name);
            io::StackOut<192> global_msg{};
            global_msg << dead_name << " died: " << ge::net::DeathReasonStr(reason);
            ge::net::ChatLine out{};
            fill_chat_line(out, ge::net::CHAT_KIND_SERVER, "SERVER", global_msg.view());
            broadcast_chat(out, now_ms);
        }

        IO_NODISCARD inline io::i32 hot_radius() const noexcept {
            return static_cast<io::i32>(stream.hot_chunks);
        }

        IO_NODISCARD inline io::i32 hot_vertical_radius() const noexcept {
            const io::i32 r = hot_radius();
            return (r < 1) ? 1 : r;
        }

        IO_NODISCARD inline bool is_chunk_hot_for_center(const ge::voxel::ChunkCoord& coord,
                                                         const ge::voxel::ChunkCoord& center) const noexcept {
            const io::i32 r = hot_radius();
            const io::i32 yr = hot_vertical_radius();
            const io::i32 dx = coord.x - center.x;
            const io::i32 dy = coord.y - center.y;
            const io::i32 dz = coord.z - center.z;
            return abs_i32(dx) <= r && abs_i32(dy) <= yr && abs_i32(dz) <= r;
        }

        IO_NODISCARD inline bool is_chunk_hot_any(const ge::voxel::ChunkCoord& coord) const noexcept {
            if (!peers) return false;
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used || !p.has_auth) continue;
                if (is_chunk_hot_for_center(coord, player_chunk(p)))
                    return true;
            }
            return false;
        }

        inline void sync_player_ecs_from_peer(io::u16 peer_index) noexcept {
            if (!player_ecs || !peers) return;
            if (peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            PeerState& p = peers[peer_index];
            if (!p.used || !p.has_auth || p.dead) {
                player_ecs->Deactivate(peer_index);
                return;
            }
            player_ecs->alive[peer_index] = 1u;
            player_ecs->transform[peer_index].x = p.auth_x;
            player_ecs->transform[peer_index].y = p.auth_y;
            player_ecs->transform[peer_index].z = p.auth_z;
            player_ecs->velocity[peer_index].x = static_cast<float>(p.move_dir_x);
            player_ecs->velocity[peer_index].y = static_cast<float>(p.move_dir_y);
            player_ecs->velocity[peer_index].z = static_cast<float>(p.move_dir_z);
            player_ecs->health[peer_index].hp = p.hp;
            player_ecs->flags[peer_index].use_fly = p.use_fly;
            player_ecs->flags[peer_index].use_noclip = p.use_noclip;
            player_ecs->flags[peer_index].grounded = p.grounded;
            player_ecs->flags[peer_index].airborne = p.airborne;
            player_ecs->runtime[peer_index].airborne_ms = p.airborne_ms;
            player_ecs->runtime[peer_index].air_peak_foot_y = p.air_peak_foot_y;
        }

        inline void sync_peer_from_player_ecs(io::u16 peer_index) noexcept {
            if (!player_ecs || !peers) return;
            if (peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            if (player_ecs->alive[peer_index] == 0u) return;
            PeerState& p = peers[peer_index];
            if (!p.used || !p.has_auth || p.dead) return;
            p.auth_x = player_ecs->transform[peer_index].x;
            p.auth_y = player_ecs->transform[peer_index].y;
            p.auth_z = player_ecs->transform[peer_index].z;
            p.hp = player_ecs->health[peer_index].hp;
            p.use_fly = player_ecs->flags[peer_index].use_fly;
            p.use_noclip = player_ecs->flags[peer_index].use_noclip;
            p.grounded = player_ecs->flags[peer_index].grounded;
            p.airborne = player_ecs->flags[peer_index].airborne;
            p.airborne_ms = player_ecs->runtime[peer_index].airborne_ms;
            p.air_peak_foot_y = player_ecs->runtime[peer_index].air_peak_foot_y;
        }

        IO_NODISCARD inline bool request_allowed(const PeerState& p, const ge::net::ChunkRequest& req) const noexcept {
            float px = p.auth_x, py = p.auth_y, pz = p.auth_z;
            if (p.has_pending) {
                px = p.pending_x;
                py = p.pending_y;
                pz = p.pending_z;
            } else if (!p.has_auth) {
                return false;
            }

            ge::voxel::ChunkCoord center{};
            io::u32 lx = 0, ly = 0, lz = 0;
            ge::voxel::split_world_coord(floor_to_i32(px), floor_to_i32(py), floor_to_i32(pz), center, lx, ly, lz);
            const io::i32 dx = req.cx - center.x;
            const io::i32 dy = req.cy - center.y;
            const io::i32 dz = req.cz - center.z;
            const io::i32 r = static_cast<io::i32>(stream.distance);
            if (dx < -r || dx > r) return false;
            if (dy < -r || dy > r) return false;
            if (dz < -r || dz > r) return false;
            return true;
        }

