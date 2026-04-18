        // World-space helpers, stream mapping, chunk updates, and sand simulation.
        ~ServerApp() noexcept { shutdown(); }

        IO_NODISCARD static inline bool endpoint_eq(const io::Endpoint& a, const io::Endpoint& b) noexcept {
            return a.addr_be == b.addr_be && a.port_be == b.port_be;
        }

        IO_NODISCARD inline io::i32 StreamYRadius() const noexcept {
            if (stream.y_radius < STREAM_Y_RADIUS_MIN) return STREAM_Y_RADIUS_MIN;
            return stream.y_radius;
        }

        IO_NODISCARD inline io::u32 WorldCycleMs() const noexcept {
            return world_time.day_ms + world_time.night_ms;
        }

        IO_NODISCARD inline io::u32 WorldPhaseMs(io::u64 now_ms) const noexcept {
            const io::u32 cycle_ms = WorldCycleMs();
            if (cycle_ms == 0u) return 0u;
            const io::u64 elapsed = (now_ms >= world_time.epoch_ms) ? (now_ms - world_time.epoch_ms) : 0u;
            io::u32 phase_ms = 0u;
            (void)io::div_u64_u32(elapsed, cycle_ms, &phase_ms);
            return phase_ms;
        }

        inline void SetWorldPhaseMs(io::u32 phase_ms, io::u64 now_ms) noexcept {
            const io::u32 cycle_ms = WorldCycleMs();
            if (cycle_ms == 0u) {
                world_time.epoch_ms = now_ms;
                return;
            }
            if (phase_ms >= cycle_ms) phase_ms %= cycle_ms;
            world_time.epoch_ms = now_ms - static_cast<io::u64>(phase_ms);
            world_time.next_broadcast_ms = 0u; // force immediate sync broadcast
        }

        IO_NODISCARD inline io::u32 SetTimeValueToPhaseMs(io::i32 value_1_to_100) const noexcept {
            io::i32 v = value_1_to_100;
            if (v < 1) v = 1;
            if (v > 100) v = 100;
            if (v <= 60) {
                const io::u32 day_span = (world_time.day_ms > 0u) ? world_time.day_ms : 1u;
                const io::u32 n = static_cast<io::u32>(v - 1);
                return (n * day_span) / 60u;
            }
            const io::u32 day_span = world_time.day_ms;
            const io::u32 night_span = (world_time.night_ms > 0u) ? world_time.night_ms : 1u;
            const io::u32 n = static_cast<io::u32>(v - 61); // 0..39
            return day_span + (n * night_span) / 40u;
        }

        IO_NODISCARD inline bool is_solid_world_cell(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            return ge::voxel::is_solid(world.get_world_block(wx, wy, wz));
        }

        IO_NODISCARD static inline float player_height_for_mode(bool crawling) noexcept {
            return crawling ? PLAYER_CRAWL_HEIGHT : PLAYER_HEIGHT;
        }

        IO_NODISCARD static inline float player_eye_to_feet_for_mode(bool crawling) noexcept {
            return crawling ? PLAYER_CRAWL_EYE_TO_FEET : PLAYER_EYE_TO_FEET;
        }

        IO_NODISCARD inline bool is_player_inside_solid(float eye_x, float eye_y, float eye_z, bool crawling = false) const noexcept {
            const float min_x = eye_x - PLAYER_RADIUS;
            const float max_x = eye_x + PLAYER_RADIUS;
            const float min_y = eye_y - player_eye_to_feet_for_mode(crawling);
            const float max_y = min_y + player_height_for_mode(crawling);
            const float min_z = eye_z - PLAYER_RADIUS;
            const float max_z = eye_z + PLAYER_RADIUS;

            const io::i32 x0 = floor_to_i32(min_x + PLAYER_COLLIDE_EPS);
            const io::i32 x1 = floor_to_i32(max_x - PLAYER_COLLIDE_EPS);
            const io::i32 y0 = floor_to_i32(min_y + PLAYER_COLLIDE_EPS);
            const io::i32 y1 = floor_to_i32(max_y - PLAYER_COLLIDE_EPS);
            const io::i32 z0 = floor_to_i32(min_z + PLAYER_COLLIDE_EPS);
            const io::i32 z1 = floor_to_i32(max_z - PLAYER_COLLIDE_EPS);

            for (io::i32 y = y0; y <= y1; ++y)
                for (io::i32 z = z0; z <= z1; ++z)
                    for (io::i32 x = x0; x <= x1; ++x)
                        if (is_solid_world_cell(x, y, z))
                            return true;
            return false;
        }

        IO_NODISCARD inline bool try_resolve_player_inside_solid(float eye_x, float eye_y, float eye_z,
                                                                 float& out_x, float& out_y, float& out_z,
                                                                 bool crawling = false) const noexcept {
            out_x = eye_x;
            out_y = eye_y;
            out_z = eye_z;
            if (!is_player_inside_solid(out_x, out_y, out_z, crawling))
                return true;

            static constexpr float STEP_UP = 0.05f;
            static constexpr io::u32 MAX_STEPS = 40u;
            for (io::u32 i = 0; i < MAX_STEPS; ++i) {
                out_y += STEP_UP;
                if (!is_player_inside_solid(out_x, out_y, out_z, crawling))
                    return true;
            }
            return false;
        }

        IO_NODISCARD inline bool is_solid_world_cell_for_ground(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            ge::voxel::BlockId id = ge::voxel::BlockId::Air;
            io::u16 state = 0u;
            if (!try_get_loaded_world_state(wx, wy, wz, id, state))
                return true;
            (void)state;
            return ge::voxel::is_solid(id);
        }

        IO_NODISCARD inline bool is_player_grounded(float eye_x, float eye_y, float eye_z, bool crawling = false) const noexcept {
            const float min_x = eye_x - PLAYER_RADIUS;
            const float max_x = eye_x + PLAYER_RADIUS;
            const float feet_y = eye_y - player_eye_to_feet_for_mode(crawling);
            const float min_z = eye_z - PLAYER_RADIUS;
            const float max_z = eye_z + PLAYER_RADIUS;

            const io::i32 x0 = floor_to_i32(min_x + PLAYER_COLLIDE_EPS);
            const io::i32 x1 = floor_to_i32(max_x - PLAYER_COLLIDE_EPS);
            const io::i32 y = floor_to_i32(feet_y - PLAYER_GROUND_PROBE);
            const io::i32 z0 = floor_to_i32(min_z + PLAYER_COLLIDE_EPS);
            const io::i32 z1 = floor_to_i32(max_z - PLAYER_COLLIDE_EPS);

            for (io::i32 z = z0; z <= z1; ++z)
                for (io::i32 x = x0; x <= x1; ++x)
                    if (is_solid_world_cell_for_ground(x, y, z))
                        return true;
            return false;
        }

        IO_NODISCARD inline io::u8* stream_ptr(io::u16 peer_index) noexcept {
            if (!stream_state || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return nullptr;
            return stream_state + static_cast<io::usize>(peer_index) * stream.count;
        }

        IO_NODISCARD inline const io::u8* stream_ptr(io::u16 peer_index) const noexcept {
            if (!stream_state || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return nullptr;
            return stream_state + static_cast<io::usize>(peer_index) * stream.count;
        }

        IO_NODISCARD inline io::u32* stream_reqid_ptr(io::u16 peer_index) noexcept {
            if (!stream_reqid || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return nullptr;
            return stream_reqid + static_cast<io::usize>(peer_index) * stream.count;
        }

        IO_NODISCARD inline io::u32* stream_sent_ms_ptr(io::u16 peer_index) noexcept {
            if (!stream_sent_ms || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return nullptr;
            return stream_sent_ms + static_cast<io::usize>(peer_index) * stream.count;
        }

        inline void stream_reset(io::u16 peer_index) noexcept {
            io::u8* ptr = stream_ptr(peer_index);
            io::u32* reqids = stream_reqid_ptr(peer_index);
            io::u32* sent_ms = stream_sent_ms_ptr(peer_index);
            if (!ptr) return;
            for (io::u32 i = 0; i < stream.count; ++i)
                ptr[i] = StreamUnsent;
            if (!reqids) return;
            for (io::u32 i = 0; i < stream.count; ++i)
                reqids[i] = 0u;
            if (!sent_ms) return;
            for (io::u32 i = 0; i < stream.count; ++i)
                sent_ms[i] = 0u;
        }

        inline void stream_set(io::u16 peer_index, io::u32 stream_slot, StreamState state) noexcept {
            io::u8* ptr = stream_ptr(peer_index);
            if (!ptr || stream_slot >= stream.count) return;
            ptr[stream_slot] = static_cast<io::u8>(state);
        }

        inline void stream_set_reqid(io::u16 peer_index, io::u32 stream_slot, io::u32 request_id) noexcept {
            io::u32* ptr = stream_reqid_ptr(peer_index);
            if (!ptr || stream_slot >= stream.count) return;
            ptr[stream_slot] = request_id;
        }

        inline void stream_set_sent_ms(io::u16 peer_index, io::u32 stream_slot, io::u32 sent_ms) noexcept {
            io::u32* ptr = stream_sent_ms_ptr(peer_index);
            if (!ptr || stream_slot >= stream.count) return;
            ptr[stream_slot] = sent_ms;
        }

        IO_NODISCARD inline io::u32 stream_get_sent_ms(io::u16 peer_index, io::u32 stream_slot) const noexcept {
            if (!stream_sent_ms || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || stream_slot >= stream.count) return 0u;
            const io::u32* ptr = stream_sent_ms + static_cast<io::usize>(peer_index) * stream.count;
            return ptr[stream_slot];
        }

        IO_NODISCARD inline io::u32 stream_get_reqid(io::u16 peer_index, io::u32 stream_slot) const noexcept {
            if (!stream_reqid || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || stream_slot >= stream.count) return 0u;
            const io::u32* ptr = stream_reqid + static_cast<io::usize>(peer_index) * stream.count;
            return ptr[stream_slot];
        }

        IO_NODISCARD inline bool find_stream_slot_by_reqid(io::u16 peer_index, io::u32 request_id, io::u32& out_slot) const noexcept {
            if (!stream_reqid) return false;
            if (peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            if (request_id == 0u || stream.count == 0u) return false;
            const io::u32* ptr = stream_reqid + static_cast<io::usize>(peer_index) * stream.count;
            for (io::u32 i = 0; i < stream.count; ++i) {
                if (ptr[i] != request_id) continue;
                out_slot = i;
                return true;
            }
            return false;
        }

        IO_NODISCARD inline bool coord_to_stream_slot(const PeerState& peer, const ge::voxel::ChunkCoord& coord, io::u32& out_slot) const noexcept {
            if (!peer.stream_center_valid) return false;
            const io::i32 ox = coord.x - peer.stream_center.x;
            const io::i32 oy = coord.y - peer.stream_center.y;
            const io::i32 oz = coord.z - peer.stream_center.z;
            const io::i32 r = static_cast<io::i32>(stream.distance);
            const io::i32 ry = StreamYRadius();
            if (ox < -r || ox > r) return false;
            if (oy < -ry || oy > ry) return false;
            if (oz < -r || oz > r) return false;
            const io::u32 ix = static_cast<io::u32>(ox + r);
            const io::u32 iy = static_cast<io::u32>(oy + ry);
            const io::u32 iz = static_cast<io::u32>(oz + r);
            out_slot = ix + iz * stream.sx + iy * stream.sx * stream.sz;
            return out_slot < stream.count;
        }

        IO_NODISCARD inline io::u32 stream_d2(io::u32 stream_slot) const noexcept {
            const io::u32 plane = stream.sx * stream.sz;
            const io::u32 iy = stream_slot / plane;
            const io::u32 rem = stream_slot % plane;
            const io::u32 iz = rem / stream.sx;
            const io::u32 ix = rem % stream.sx;
            const io::i32 ox = static_cast<io::i32>(ix) - static_cast<io::i32>(stream.distance);
            const io::i32 oy = static_cast<io::i32>(iy) - StreamYRadius();
            const io::i32 oz = static_cast<io::i32>(iz) - static_cast<io::i32>(stream.distance);
            return static_cast<io::u32>(ox * ox + oy * oy + oz * oz);
        }

        IO_NODISCARD inline io::u32 stream_priority(io::u32 stream_slot) const noexcept {
            const io::u32 plane = stream.sx * stream.sz;
            const io::u32 iy = stream_slot / plane;
            const io::u32 rem = stream_slot % plane;
            const io::u32 iz = rem / stream.sx;
            const io::u32 ix = rem % stream.sx;
            const io::i32 ox = static_cast<io::i32>(ix) - static_cast<io::i32>(stream.distance);
            const io::i32 oy = static_cast<io::i32>(iy) - StreamYRadius();
            const io::i32 oz = static_cast<io::i32>(iz) - static_cast<io::i32>(stream.distance);
            return ge::server::chunk::StreamPriority(ox, oy, oz);
        }

        IO_NODISCARD inline io::u32 count_peer_inflight_streams(io::u16 peer_index) const noexcept {
            const io::u8* states = stream_ptr(peer_index);
            return ge::server::chunk::CountInflight(states, stream.count, StreamQueued, StreamAwaitAck);
        }

        inline void stream_offset(io::u32 stream_slot, io::i32& ox, io::i32& oy, io::i32& oz) const noexcept {
            const io::u32 plane = stream.sx * stream.sz;
            const io::u32 iy = stream_slot / plane;
            const io::u32 rem = stream_slot % plane;
            const io::u32 iz = rem / stream.sx;
            const io::u32 ix = rem % stream.sx;
            ox = static_cast<io::i32>(ix) - static_cast<io::i32>(stream.distance);
            oy = static_cast<io::i32>(iy) - StreamYRadius();
            oz = static_cast<io::i32>(iz) - static_cast<io::i32>(stream.distance);
        }

        inline void upsert_world_chunk(const ge::voxel::ChunkData& chunk) noexcept {
            ge::voxel::ChunkData* dst = world.ensure_chunk(chunk.coord);
            if (!dst) return;
            copy_chunk_data(*dst, chunk);
            dst->coord = chunk.coord;
            dst->generated = true;
            dst->dirty_mesh = false;
            dst->dirty_neighbors = false;
        }

        IO_NODISCARD inline ge::voxel::ChunkData* ensure_world_chunk(const ge::voxel::ChunkCoord& coord) noexcept {
            ge::voxel::ChunkData* cached = world.find_chunk(coord);
            if (cached) return cached;

            ge::voxel::ChunkData* dst = world.ensure_chunk(coord);
            if (!dst) return nullptr;

            if (!ge::voxel::load_chunk_binary(coord, *dst)) {
                if (!terrain) {
                    (void)world.remove_chunk(coord);
                    return nullptr;
                }
                dst->coord = coord;
                terrain_lock.lock();
                terrain->generate_chunk(*dst);
                terrain_lock.unlock();
            }

            dst->coord = coord;
            dst->generated = true;
            dst->dirty_mesh = false;
            dst->dirty_neighbors = false;
            load_chunk_item_drops(coord, io::monotonic_ms());
            return dst;
        }

        struct ChunkItemDiskHeader {
            io::u32 magic = 0x52445449u; // "ITDR"
            io::u32 version = 1u;
            io::i32 cx{};
            io::i32 cy{};
            io::i32 cz{};
            io::u16 count{};
            io::u16 _pad{};
        };

        struct ChunkItemDiskEntry {
            io::u32 px_bits{};
            io::u32 py_bits{};
            io::u32 pz_bits{};
            io::u32 vx_bits{};
            io::u32 vy_bits{};
            io::u32 vz_bits{};
            io::u16 item_id{};
            io::u16 count{};
            io::u16 freshness{};
            io::u16 _pad0{};
            io::u32 pile_count{};
            io::u32 despawn_ms{};
            io::u8 grounded{};
            io::u8 _pad[3]{};
        };

        static inline void chunk_item_file_name(const ge::voxel::ChunkCoord& coord, io::StackOut<96>& out) noexcept {
            out.reset();
            out << "c_" << coord.x << "_" << coord.y << "_" << coord.z << ".items.bin";
        }

        template<io::usize N>
        IO_NODISCARD static inline bool build_chunk_item_file_path_stack(const ge::voxel::ChunkCoord& coord, io::StackOut<N>& out_path) noexcept {
            io::StackOut<N> chunks_root{};
            if (!ge::voxel::ensure_chunk_storage_root_stack(chunks_root)) return false;
            io::StackOut<96> leaf{};
            chunk_item_file_name(coord, leaf);
            out_path.reset();
            fs::path_join(chunks_root.view(), leaf.view(), out_path);
            return !out_path.view().empty();
        }

        inline void despawn_item_actors_in_chunk(const ge::voxel::ChunkCoord& coord) noexcept {
            if (!world_actor_ecs) return;
            ActorEcs& ecs = *world_actor_ecs;
            for (io::u16 i = 0u; i < WORLD_ACTOR_CAP; ++i) {
                if (ecs.alive[i] == 0u) continue;
                if (!ecs.net_sync[i].active) continue;
                if (ecs.identity[i].model != ge::net::WORLD_ACTOR_MODEL_ITEM) continue;
                ge::voxel::ChunkCoord cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(
                    floor_to_i32(ecs.transform[i].x),
                    floor_to_i32(ecs.transform[i].y),
                    floor_to_i32(ecs.transform[i].z),
                    cc, lx, ly, lz);
                if (!ge::voxel::coord_eq(cc, coord)) continue;
                despawn_item_actor(i);
            }
        }

        inline void save_chunk_item_drops(const ge::voxel::ChunkCoord& coord) noexcept {
            if (!world_actor_ecs) return;
            io::spin_mutex& lock = ge::voxel::chunk_storage_lock();
            lock.lock();
            io::StackOut<384> path{};
            if (!build_chunk_item_file_path_stack(coord, path)) {
                lock.unlock();
                return;
            }

            // Keep this off stack: ReleaseMini x86 runs without stack probes and
            // large local frames can cause guard-page jumps (C0000005 instead of
            // clean stack overflow).
            static ChunkItemDiskEntry ring[WORLD_ITEMS_PER_CHUNK_CAP]{};
            io::u32 ring_head = 0u;
            io::u32 ring_count = 0u;
            ActorEcs& ecs = *world_actor_ecs;
            for (io::u16 i = 0u; i < WORLD_ACTOR_CAP; ++i) {
                if (ecs.alive[i] == 0u) continue;
                if (!ecs.net_sync[i].active) continue;
                if (ecs.identity[i].model != ge::net::WORLD_ACTOR_MODEL_ITEM) continue;
                ge::voxel::ChunkCoord cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(
                    floor_to_i32(ecs.transform[i].x),
                    floor_to_i32(ecs.transform[i].y),
                    floor_to_i32(ecs.transform[i].z),
                    cc, lx, ly, lz);
                if (!ge::voxel::coord_eq(cc, coord)) continue;
                ge::item::Stack st = ecs.item_drop[i].stack;
                ge::item::normalize(st);
                if (ge::item::is_empty(st)) continue;
                ChunkItemDiskEntry e{};
                e.px_bits = ge::net::f32_to_bits(ecs.transform[i].x);
                e.py_bits = ge::net::f32_to_bits(ecs.transform[i].y);
                e.pz_bits = ge::net::f32_to_bits(ecs.transform[i].z);
                e.vx_bits = ge::net::f32_to_bits(ecs.velocity[i].x);
                e.vy_bits = ge::net::f32_to_bits(ecs.velocity[i].y);
                e.vz_bits = ge::net::f32_to_bits(ecs.velocity[i].z);
                e.item_id = static_cast<io::u16>(st.id);
                e.count = st.count;
                e.freshness = st.freshness;
                e.pile_count = ecs.item_drop[i].pile_count;
                if (e.pile_count == 0u) e.pile_count = st.count;
                e.despawn_ms = ecs.item_drop[i].despawn_ms;
                e.grounded = ecs.item_drop[i].grounded ? 1u : 0u;
                if (ring_count < WORLD_ITEMS_PER_CHUNK_CAP) {
                    ring[ring_count++] = e;
                } else {
                    ring[ring_head] = e;
                    ring_head = (ring_head + 1u) % WORLD_ITEMS_PER_CHUNK_CAP;
                }
            }

            fs::File f{ path.view(), io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Truncate | io::OpenMode::Binary };
            if (!f.is_open()) {
                lock.unlock();
                return;
            }
            ChunkItemDiskHeader h{};
            h.cx = coord.x;
            h.cy = coord.y;
            h.cz = coord.z;
            h.count = static_cast<io::u16>(ring_count > WORLD_ITEMS_PER_CHUNK_CAP ? WORLD_ITEMS_PER_CHUNK_CAP : ring_count);
            const io::char_view hbytes{ reinterpret_cast<const char*>(&h), sizeof(h) };
            if (f.write(hbytes) != hbytes.size()) {
                lock.unlock();
                return;
            }
            for (io::u32 i = 0u; i < ring_count; ++i) {
                const io::u32 idx = (ring_count < WORLD_ITEMS_PER_CHUNK_CAP) ? i : ((ring_head + i) % WORLD_ITEMS_PER_CHUNK_CAP);
                const io::char_view ebytes{ reinterpret_cast<const char*>(&ring[idx]), sizeof(ChunkItemDiskEntry) };
                if (f.write(ebytes) != ebytes.size()) {
                    lock.unlock();
                    return;
                }
            }
            (void)f.flush();
            lock.unlock();
        }

        inline void load_chunk_item_drops(const ge::voxel::ChunkCoord& coord, io::u64 now_ms) noexcept {
            if (!world_actor_ecs) return;
            io::spin_mutex& lock = ge::voxel::chunk_storage_lock();
            lock.lock();
            io::StackOut<384> path{};
            if (!build_chunk_item_file_path_stack(coord, path) || !fs::exists(path.view())) {
                lock.unlock();
                return;
            }
            fs::File f{ path.view(), io::OpenMode::Read | io::OpenMode::Binary };
            if (!f.is_open()) {
                lock.unlock();
                return;
            }
            ChunkItemDiskHeader h{};
            if (!f.read_exact(&h, sizeof(h))) {
                lock.unlock();
                return;
            }
            if (h.magic != 0x52445449u || h.version != 1u || h.cx != coord.x || h.cy != coord.y || h.cz != coord.z) {
                lock.unlock();
                return;
            }
            io::u32 count = h.count;
            if (count > WORLD_ITEMS_PER_CHUNK_CAP) count = WORLD_ITEMS_PER_CHUNK_CAP;
            for (io::u32 i = 0u; i < count; ++i) {
                ChunkItemDiskEntry e{};
                if (!f.read_exact(&e, sizeof(e)))
                    break;
                ge::item::Id id = static_cast<ge::item::Id>(e.item_id);
                if (!ge::item::valid(id) || id == ge::item::Id::None)
                    continue;
                ge::item::Stack stack = ge::item::make_stack(id, e.count, e.freshness);
                ge::item::normalize(stack);
                if (ge::item::is_empty(stack))
                    continue;
                const io::u32 pile = (e.pile_count == 0u) ? stack.count : e.pile_count;
                const float x = ge::net::bits_to_f32(e.px_bits);
                const float y = ge::net::bits_to_f32(e.py_bits);
                const float z = ge::net::bits_to_f32(e.pz_bits);
                const float vx = ge::net::bits_to_f32(e.vx_bits);
                const float vy = ge::net::bits_to_f32(e.vy_bits);
                const float vz = ge::net::bits_to_f32(e.vz_bits);

                ActorEcs& ecs = *world_actor_ecs;
                const io::i32 slot = ecs.Spawn(ge::net::WORLD_ACTOR_MODEL_ITEM, ge::net::WORLD_ACTOR_MODE_ENTITY);
                if (slot < 0) break;
                const io::u32 actor = static_cast<io::u32>(slot);
                ecs.transform[actor].x = x;
                ecs.transform[actor].y = y;
                ecs.transform[actor].z = z;
                ecs.velocity[actor].x = vx;
                ecs.velocity[actor].y = vy;
                ecs.velocity[actor].z = vz;
                ecs.mob_state[actor].logical = ge::ecs::MobLogicState::Idle;
                ecs.mob_state[actor].net_state = static_cast<io::u8>(stack.id);
                ecs.mob_state[actor].net_anim = static_cast<io::u8>(ge::item::freshness_band(stack));
                ecs.item_drop[actor].stack = stack;
                ecs.item_drop[actor].pile_count = pile;
                ecs.item_drop[actor].grounded = e.grounded != 0u;
                ecs.item_drop[actor].despawn_ms = (e.despawn_ms == 0u) ? WORLD_ITEM_DESPAWN_MS : e.despawn_ms;
                ecs.anchor[actor] = {};
                ecs.net_sync[actor].active = true;
                ecs.net_sync[actor].dirty = true;
                ecs.net_sync[actor].last_update_ms = now_ms;
            }
            lock.unlock();
        }

        static inline void copy_chunk_data(ge::voxel::ChunkData& dst, const ge::voxel::ChunkData& src) noexcept {
            dst.coord = src.coord;
            dst.non_air_count = src.non_air_count;
            dst.version = src.version;
            dst.dirty_mesh = src.dirty_mesh;
            dst.dirty_neighbors = src.dirty_neighbors;
            dst.generated = src.generated;
            for (io::u32 i = 0; i < ge::voxel::CHUNK_VOLUME; ++i)
                dst.blocks[i] = src.blocks[i];
        }

        IO_NODISCARD static inline bool is_book_anchor(ge::voxel::BlockId id) noexcept {
            return id == ge::voxel::BlockId::LevitatingBookAnchor;
        }

        IO_NODISCARD inline io::i32 find_actor_slot_by_anchor(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            if (!world_actor_ecs) return -1;
            return world_actor_ecs->FindByAnchor(wx, wy, wz);
        }

        IO_NODISCARD inline io::i32 find_actor_slot_by_id(io::u16 actor_id) const noexcept {
            if (!world_actor_ecs) return -1;
            return world_actor_ecs->FindByActorId(actor_id);
        }

        IO_NODISCARD inline io::i32 find_free_actor_slot() const noexcept {
            if (!world_actor_ecs) return -1;
            return world_actor_ecs->FindFree();
        }

        inline io::u16 alloc_actor_id() noexcept {
            if (!world_actor_ecs) return 0u;
            return world_actor_ecs->AllocActorId();
        }

        inline void mark_actor_dirty(io::u32 actor_index) noexcept {
            if (!world_actor_ecs) return;
            world_actor_ecs->MarkDirty(actor_index);
        }

        inline bool spawn_book_entity_anchor(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
            if (!world_actor_ecs) return false;
            ActorEcs& ecs = *world_actor_ecs;
            io::i32 slot = find_actor_slot_by_anchor(wx, wy, wz);
            if (slot < 0) {
                slot = ecs.Spawn(ge::net::WORLD_ACTOR_MODEL_LEVITATING_BOOK, ge::net::WORLD_ACTOR_MODE_ENTITY);
                if (slot < 0) return false;
            }

            const io::u32 i = static_cast<io::u32>(slot);
            if (ecs.alive[i] == 0u) return false;
            ecs.identity[i].model = ge::net::WORLD_ACTOR_MODEL_LEVITATING_BOOK;
            ecs.identity[i].mode = ge::net::WORLD_ACTOR_MODE_ENTITY;
            ecs.transform[i].x = static_cast<float>(wx) + 0.5f;
            ecs.transform[i].y = static_cast<float>(wy) + 0.25f;
            ecs.transform[i].z = static_cast<float>(wz) + 0.5f;
            ecs.velocity[i] = {};
            ecs.mob_state[i].logical = ge::ecs::MobLogicState::Idle;
            ecs.mob_state[i].net_state = ge::net::WORLD_ACTOR_STATE_ENTITY_STAY;
            ecs.mob_state[i].net_anim = ge::net::WORLD_ACTOR_ANIM_STAY;
            ecs.anchor[i].has_anchor = true;
            ecs.anchor[i].wx = wx;
            ecs.anchor[i].wy = wy;
            ecs.anchor[i].wz = wz;
            ecs.net_sync[i].active = true;
            ecs.net_sync[i].last_update_ms = now_ms;
            mark_actor_dirty(i);
            return true;
        }

        inline bool convert_book_anchor_to_mob(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
            if (!world_actor_ecs) return false;
            ActorEcs& ecs = *world_actor_ecs;
            io::i32 slot = find_actor_slot_by_anchor(wx, wy, wz);
            if (slot < 0) {
                slot = ecs.Spawn(ge::net::WORLD_ACTOR_MODEL_LEVITATING_BOOK, ge::net::WORLD_ACTOR_MODE_MOB);
                if (slot < 0) return false;
            }

            const io::u32 i = static_cast<io::u32>(slot);
            if (ecs.alive[i] == 0u) return false;
            ecs.identity[i].mode = ge::net::WORLD_ACTOR_MODE_MOB;
            ecs.identity[i].model = ge::net::WORLD_ACTOR_MODEL_LEVITATING_BOOK;
            ecs.transform[i].x = static_cast<float>(wx) + 0.5f;
            ecs.transform[i].y = static_cast<float>(wy) + 0.35f;
            ecs.transform[i].z = static_cast<float>(wz) + 0.5f;
            ecs.velocity[i] = {};
            ecs.mob_sim[i].target_peer = -1;
            ecs.mob_sim[i].grounded = false;
            ecs.mob_sim[i].ai_state = ge::ecs::MobLogicState::Idle;
            ecs.mob_state[i].logical = ge::ecs::MobLogicState::Idle;
            ecs.mob_state[i].net_state = ge::net::WORLD_ACTOR_STATE_MOB_IDLE;
            ecs.mob_state[i].net_anim = ge::net::WORLD_ACTOR_ANIM_STAY;
            ecs.anchor[i] = {};
            ecs.net_sync[i].active = true;
            ecs.net_sync[i].last_update_ms = now_ms;
            mark_actor_dirty(i);
            return true;
        }

        inline bool despawn_book_anchor_actor(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
            (void)now_ms;
            if (!world_actor_ecs) return false;
            const io::i32 slot = find_actor_slot_by_anchor(wx, wy, wz);
            if (slot < 0) return false;
            world_actor_ecs->MarkInactive(static_cast<io::u32>(slot));
            return true;
        }

        IO_NODISCARD inline bool block_edit_allowed(const PeerState& p, const ge::net::BlockEdit& edit) const noexcept {
            if (!p.has_auth) return false;

            float px = p.auth_x;
            float py = p.auth_y;
            float pz = p.auth_z;
            if (p.has_pending) {
                px = p.pending_x;
                py = p.pending_y;
                pz = p.pending_z;
            }

            const float bx = static_cast<float>(edit.wx) + 0.5f;
            const float by = static_cast<float>(edit.wy) + 0.5f;
            const float bz = static_cast<float>(edit.wz) + 0.5f;
            const float dx = bx - px;
            const float dy = by - py;
            const float dz = bz - pz;
            const float reach = BLOCK_EDIT_REACH + (p.has_pending ? 1.25f : 0.f);
            const float reach2 = reach * reach;
            if (dx * dx + dy * dy + dz * dz > reach2)
                return false;

            // Reach check above is authoritative enough for edits.
            // Avoid coupling gameplay edits to transient stream windows,
            // otherwise valid nearby edits can be rejected while crossing chunk borders.
            return true;
        }

        inline bool apply_block_edit_world(const ge::net::BlockEdit& edit, bool save_to_disk = true) noexcept {
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0, ly = 0, lz = 0;
            ge::voxel::split_world_coord(edit.wx, edit.wy, edit.wz, cc, lx, ly, lz);
            // Block edits are gameplay-critical.
            // Ensure the target chunk exists in world cache (load from disk or generate once)
            // so edits are not randomly rejected just because hot-cache pruned it.
            ge::voxel::ChunkData* current = ensure_world_chunk(cc);
            if (!current) return false;

            ge::voxel::BlockId id = ge::voxel::BlockId::Air;
            if (edit.block_id < ge::voxel::BLOCK_COUNT)
                id = static_cast<ge::voxel::BlockId>(edit.block_id);
            io::u16 next_state = edit.state;
            // Layered fluids: normalize to packed two-layer state.
            if (ge::voxel::is_fluid_block_id(id)) {
                ge::voxel::FluidStack s{};
                s.bottom_kind = ge::voxel::fluid_kind_from_block_id(id);
                io::u8 level = static_cast<io::u8>(next_state & 0x0Fu);
                if (level == 0u) level = ge::voxel::FLUID_LEVEL_MAX;
                s.bottom_level = level;
                ge::voxel::normalize_fluid_stack(s);
                ge::voxel::fluid_stack_to_block(s, id, next_state);
            }
            const ge::voxel::BlockId prev_id = world.get_world_block(edit.wx, edit.wy, edit.wz);
            const io::u64 now_ms = io::monotonic_ms();
            if (!world.set_world_state(edit.wx, edit.wy, edit.wz, id, next_state, false))
                return false;
            hot_update_flags_cell(edit.wx, edit.wy, edit.wz, id, now_ms);
            enqueue_sand_neighborhood(edit.wx, edit.wy, edit.wz, now_ms);
            enqueue_water_neighborhood(edit.wx, edit.wy, edit.wz, now_ms);

            if (is_book_anchor(id) && !is_book_anchor(prev_id))
                (void)spawn_book_entity_anchor(edit.wx, edit.wy, edit.wz, now_ms);
            else if (!is_book_anchor(id) && is_book_anchor(prev_id)) {
                if (id == ge::voxel::BlockId::Air)
                    (void)convert_book_anchor_to_mob(edit.wx, edit.wy, edit.wz, now_ms);
                else
                    (void)despawn_book_anchor_actor(edit.wx, edit.wy, edit.wz, now_ms);
            }

            ge::voxel::ChunkData* changed = world.find_chunk(cc);
            if (!changed) return false;
            changed->generated = true;
            changed->dirty_mesh = false;
            changed->dirty_neighbors = false;
            if (save_to_disk) {
                if (!ge::voxel::save_chunk_binary(*changed)) {
#ifdef _DEBUG
                    io::out << "[server] failed to save chunk (" << cc.x << "," << cc.y << "," << cc.z << ")\n";
#endif
                }
            }
            return true;
        }

        IO_NODISCARD inline bool try_get_loaded_world_state(io::i32 wx, io::i32 wy, io::i32 wz,
                                                            ge::voxel::BlockId& out_id,
                                                            io::u16& out_state) const noexcept {
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0, ly = 0, lz = 0;
            ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            const ge::voxel::ChunkData* chunk = world.find_chunk(cc);
            if (!chunk) return false;
            const ge::voxel::BlockState state = chunk->get_state_at(lx, ly, lz);
            if (state.id < ge::voxel::BLOCK_COUNT) out_id = static_cast<ge::voxel::BlockId>(state.id);
            else out_id = ge::voxel::BlockId::Air;
            out_state = state.state;
            return true;
        }

        IO_NODISCARD inline bool try_get_world_state_for_sand(io::i32 wx, io::i32 wy, io::i32 wz,
                                                               ge::voxel::BlockId& out_id,
                                                               io::u16& out_state) const noexcept {
            if (try_get_loaded_world_state(wx, wy, wz, out_id, out_state))
                return true;
            out_id = ge::voxel::BlockId::Air;
            out_state = 0u;
            return true;
        }

        IO_NODISCARD static inline bool sand_can_move_into(ge::voxel::BlockId id) noexcept {
            return id == ge::voxel::BlockId::Air || ge::voxel::is_liquid(id);
        }

        IO_NODISCARD static inline bool is_water_block_id(ge::voxel::BlockId id) noexcept {
            return ge::voxel::is_fluid_block_id(id);
        }

        IO_NODISCARD static inline bool is_water_source_active(ge::voxel::BlockId id, io::u16 state) noexcept {
            (void)id;
            (void)state;
            // Noita-like mode: no infinite water sources.
            return false;
        }

        IO_NODISCARD static inline io::u16 water_flow_level_from_state(io::u16 state) noexcept {
            const ge::voxel::FluidStack s = ge::voxel::unpack_fluid_stack_state(state);
            return ge::voxel::fluid_total_level(s);
        }

        IO_NODISCARD static inline io::u16 water_level_for_state(ge::voxel::BlockId id, io::u16 state) noexcept {
            return ge::voxel::fluid_total_level_from_block(id, state);
        }

        IO_NODISCARD static inline bool water_is_fillable(ge::voxel::BlockId id) noexcept {
            return id == ge::voxel::BlockId::Air || ge::voxel::is_fluid_block_id(id);
        }

        IO_NODISCARD static inline io::u16 make_water_flow_state(io::u16 level) noexcept {
            ge::voxel::FluidStack s{};
            s.bottom_kind = ge::voxel::FluidKind::Water;
            s.bottom_level = static_cast<io::u8>(level);
            return ge::voxel::pack_fluid_stack_state(s);
        }

        IO_NODISCARD static inline ge::voxel::BlockId water_id_for_level(io::u16 level) noexcept {
            if (level == 0u) return ge::voxel::BlockId::Air;
            return ge::voxel::BlockId::WaterDark;
        }

        IO_NODISCARD static inline io::u16 water_state_for_level(io::u16 level) noexcept {
            if (level == 0u) return 0u;
            return make_water_flow_state(level);
        }

        IO_NODISCARD static inline io::u16 make_water_source_state(bool enabled) noexcept {
            return enabled ? 0u : WATER_SOURCE_DISABLED_FLAG;
        }

        IO_NODISCARD inline bool try_get_world_state_for_water(io::i32 wx, io::i32 wy, io::i32 wz,
                                                                ge::voxel::BlockId& out_id,
                                                                io::u16& out_state) const noexcept {
            if (try_get_loaded_world_state(wx, wy, wz, out_id, out_state))
                return true;
            out_id = ge::voxel::BlockId::Air;
            out_state = 0u;
            return true;
        }

        IO_NODISCARD static inline io::u8 hot_block_flags_for_id(ge::voxel::BlockId id) noexcept {
            io::u8 flags = 0u;
            if (ge::voxel::is_solid(id))
                flags |= HOT_FLAG_HAS_COLLISION;
            if (id == ge::voxel::BlockId::Sand)
                flags |= HOT_FLAG_HAS_FALL_SIM;
            if (ge::voxel::is_liquid(id))
                flags |= HOT_FLAG_CAN_PLAYER_SWIM;
            // Reserved for future rare lookups (lava, acid, cursed cells, etc).
            if (id == ge::voxel::BlockId::LevitatingBookAnchor)
                flags |= HOT_FLAG_HAS_SPECIAL;
            return flags;
        }

        static inline void hot_pack_set(io::u8* packed, io::u32 linear, io::u8 flags) noexcept {
            const io::u32 byte_index = linear >> 1u;
            const io::u8 nibble = static_cast<io::u8>(flags & 0x0Fu);
            if ((linear & 1u) == 0u)
                packed[byte_index] = static_cast<io::u8>((packed[byte_index] & 0xF0u) | nibble);
            else
                packed[byte_index] = static_cast<io::u8>((packed[byte_index] & 0x0Fu) | static_cast<io::u8>(nibble << 4u));
        }

        IO_NODISCARD static inline io::u8 hot_pack_get(const io::u8* packed, io::u32 linear) noexcept {
            const io::u8 b = packed[linear >> 1u];
            if ((linear & 1u) == 0u)
                return static_cast<io::u8>(b & 0x0Fu);
            return static_cast<io::u8>((b >> 4u) & 0x0Fu);
        }

        IO_NODISCARD static inline bool hot_bit_test(const io::u8* bits, io::u32 linear) noexcept {
            const io::u32 idx = linear >> 3u;
            const io::u8 mask = static_cast<io::u8>(1u << (linear & 7u));
            return (bits[idx] & mask) != 0u;
        }

        static inline void hot_bit_set(io::u8* bits, io::u32 linear) noexcept {
            const io::u32 idx = linear >> 3u;
            const io::u8 mask = static_cast<io::u8>(1u << (linear & 7u));
            bits[idx] = static_cast<io::u8>(bits[idx] | mask);
        }

        static inline void hot_bit_clear(io::u8* bits, io::u32 linear) noexcept {
            const io::u32 idx = linear >> 3u;
            const io::u8 mask = static_cast<io::u8>(1u << (linear & 7u));
            bits[idx] = static_cast<io::u8>(bits[idx] & static_cast<io::u8>(~mask));
        }

        inline void rebuild_hot_chunk_sim(HotChunkSim& e, const ge::voxel::ChunkData& chunk, io::u64 now_ms) noexcept {
            e.coord = chunk.coord;
            e.chunk_version = chunk.version;
            e.last_touch_ms = now_ms;
            for (io::u32 i = 0u; i < HOT_BLOCK_PACKED_BYTES; ++i)
                e.packed_flags[i] = 0u;
            for (io::u32 i = 0u; i < ge::voxel::CHUNK_VOLUME; ++i) {
                const io::u16 raw = chunk.blocks[i].id;
                ge::voxel::BlockId id = ge::voxel::BlockId::Air;
                if (raw < ge::voxel::BLOCK_COUNT)
                    id = static_cast<ge::voxel::BlockId>(raw);
                hot_pack_set(e.packed_flags, i, hot_block_flags_for_id(id));
            }
            e.used = true;
        }

        IO_NODISCARD inline io::u32 find_hot_chunk_sim_slot(const ge::voxel::ChunkCoord& coord) const noexcept {
            if (!hot_sim || hot_sim_cap == 0u) return 0xFFFFFFFFu;
            if (hot_sim_last_idx < hot_sim_cap) {
                const HotChunkSim& c = hot_sim[hot_sim_last_idx];
                if (c.used && ge::voxel::coord_eq(c.coord, coord))
                    return hot_sim_last_idx;
            }
            for (io::u32 i = 0u; i < hot_sim_cap; ++i) {
                const HotChunkSim& c = hot_sim[i];
                if (!c.used) continue;
                if (ge::voxel::coord_eq(c.coord, coord))
                    return i;
            }
            return 0xFFFFFFFFu;
        }

        IO_NODISCARD inline io::u32 alloc_hot_chunk_sim_slot(const ge::voxel::ChunkCoord& coord, io::u64 now_ms) noexcept {
            if (!hot_sim || hot_sim_cap == 0u) return 0xFFFFFFFFu;
            for (io::u32 i = 0u; i < hot_sim_cap; ++i) {
                if (!hot_sim[i].used) {
                    hot_sim[i].used = true;
                    hot_sim[i].coord = coord;
                    hot_sim[i].chunk_version = 0u;
                    hot_sim[i].last_touch_ms = now_ms;
                    for (io::u32 b = 0u; b < ((ge::voxel::CHUNK_VOLUME + 7u) / 8u); ++b) {
                        hot_sim[i].sand_queued[b] = 0u;
                        hot_sim[i].water_queued[b] = 0u;
                    }
                    hot_sim_last_idx = i;
                    return i;
                }
            }
            io::u32 victim = 0xFFFFFFFFu;
            io::u64 oldest_ms = static_cast<io::u64>(-1);
            for (io::u32 i = 0u; i < hot_sim_cap; ++i) {
                const HotChunkSim& c = hot_sim[i];
                if (!is_chunk_hot_any(c.coord)) {
                    if (c.last_touch_ms <= oldest_ms) {
                        oldest_ms = c.last_touch_ms;
                        victim = i;
                    }
                }
            }
            if (victim == 0xFFFFFFFFu) {
                for (io::u32 i = 0u; i < hot_sim_cap; ++i) {
                    const HotChunkSim& c = hot_sim[i];
                    if (c.last_touch_ms <= oldest_ms) {
                        oldest_ms = c.last_touch_ms;
                        victim = i;
                    }
                }
            }
            if (victim == 0xFFFFFFFFu) return 0xFFFFFFFFu;
            hot_sim[victim].used = true;
            hot_sim[victim].coord = coord;
            hot_sim[victim].chunk_version = 0u;
            hot_sim[victim].last_touch_ms = now_ms;
            for (io::u32 b = 0u; b < ((ge::voxel::CHUNK_VOLUME + 7u) / 8u); ++b) {
                hot_sim[victim].sand_queued[b] = 0u;
                hot_sim[victim].water_queued[b] = 0u;
            }
            hot_sim_last_idx = victim;
            return victim;
        }

        IO_NODISCARD inline HotChunkSim* ensure_hot_chunk_sim(const ge::voxel::ChunkCoord& coord, io::u64 now_ms) noexcept {
            if (!hot_sim || hot_sim_cap == 0u) return nullptr;
            io::u32 idx = find_hot_chunk_sim_slot(coord);
            if (idx == 0xFFFFFFFFu)
                idx = alloc_hot_chunk_sim_slot(coord, now_ms);
            if (idx == 0xFFFFFFFFu) return nullptr;
            hot_sim_last_idx = idx;
            HotChunkSim& slot = hot_sim[idx];
            slot.last_touch_ms = now_ms;
            ge::voxel::ChunkData* world_chunk = world.find_chunk(coord);
            if (!world_chunk)
                world_chunk = ensure_world_chunk(coord);
            if (!world_chunk) return nullptr;
            if (!slot.used || slot.chunk_version != world_chunk->version || !ge::voxel::coord_eq(slot.coord, coord))
                rebuild_hot_chunk_sim(slot, *world_chunk, now_ms);
            return &slot;
        }

        IO_NODISCARD inline bool hot_get_flags(io::i32 wx, io::i32 wy, io::i32 wz, io::u8& out_flags, io::u64 now_ms) noexcept {
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            if (!is_chunk_hot_any(cc)) return false;
            HotChunkSim* chunk = ensure_hot_chunk_sim(cc, now_ms);
            if (!chunk) return false;
            const io::u32 linear = ge::voxel::chunk_index(lx, ly, lz);
            out_flags = hot_pack_get(chunk->packed_flags, linear);
            return true;
        }

        inline void hot_update_flags_cell(io::i32 wx, io::i32 wy, io::i32 wz, ge::voxel::BlockId id, io::u64 now_ms) noexcept {
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            if (!is_chunk_hot_any(cc)) return;
            HotChunkSim* chunk = ensure_hot_chunk_sim(cc, now_ms);
            if (!chunk) return;
            const io::u32 linear = ge::voxel::chunk_index(lx, ly, lz);
            hot_pack_set(chunk->packed_flags, linear, hot_block_flags_for_id(id));
        }

        IO_NODISCARD inline bool hot_cell_is_collision(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
            io::u8 flags = 0u;
            if (!hot_get_flags(wx, wy, wz, flags, now_ms)) return true;
            return (flags & HOT_FLAG_HAS_COLLISION) != 0u;
        }

        IO_NODISCARD inline bool hot_cell_is_swimmable(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
            io::u8 flags = 0u;
            if (!hot_get_flags(wx, wy, wz, flags, now_ms)) return false;
            return (flags & HOT_FLAG_CAN_PLAYER_SWIM) != 0u;
        }

        IO_NODISCARD static inline bool is_reserved_cell(const SandReservedCell* cells, io::u32 count,
                                                         io::i32 x, io::i32 y, io::i32 z) noexcept {
            for (io::u32 i = 0; i < count; ++i) {
                if (cells[i].x == x && cells[i].y == y && cells[i].z == z)
                    return true;
            }
            return false;
        }

        static inline void reserve_cell(SandReservedCell* cells, io::u32& count,
                                        io::i32 x, io::i32 y, io::i32 z) noexcept {
            if (count >= SAND_MOVE_BUDGET) return;
            cells[count++] = SandReservedCell{ x, y, z };
        }

        inline void enqueue_sand_cell(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms = 0u) noexcept {
            if (!sand_active) return;
            if (now_ms == 0u) now_ms = io::monotonic_ms();
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            if (!is_chunk_hot_any(cc)) return;
            HotChunkSim* hs = nullptr;
            const io::u32 si = find_hot_chunk_sim_slot(cc);
            if (si != 0xFFFFFFFFu && hot_sim[si].used)
                hs = &hot_sim[si];
            else
                hs = ensure_hot_chunk_sim(cc, now_ms);
            if (!hs) return;
            const io::u32 linear = ge::voxel::chunk_index(lx, ly, lz);
            if (hot_bit_test(hs->sand_queued, linear))
                return;
            hot_bit_set(hs->sand_queued, linear);
            if (stats.sand_active_count >= SAND_ACTIVE_CAP) {
                const SandActiveCell drop = sand_active[stats.sand_active_head];
                ge::voxel::ChunkCoord dcc{};
                ge::voxel::split_world_coord(drop.wx, drop.wy, drop.wz, dcc, lx, ly, lz);
                const io::u32 di = find_hot_chunk_sim_slot(dcc);
                if (di != 0xFFFFFFFFu && hot_sim[di].used) {
                    const io::u32 dlin = ge::voxel::chunk_index(lx, ly, lz);
                    hot_bit_clear(hot_sim[di].sand_queued, dlin);
                }
                stats.sand_active_head = (stats.sand_active_head + 1u) % SAND_ACTIVE_CAP;
                --stats.sand_active_count;
            }
            sand_active[stats.sand_active_tail] = SandActiveCell{ wx, wy, wz };
            stats.sand_active_tail = (stats.sand_active_tail + 1u) % SAND_ACTIVE_CAP;
            ++stats.sand_active_count;
        }

        inline void enqueue_sand_neighborhood(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms = 0u) noexcept {
            for (io::i32 dx = -1; dx <= 1; ++dx)
                for (io::i32 dz = -1; dz <= 1; ++dz)
                    enqueue_sand_cell(wx + dx, wy, wz + dz, now_ms);

            for (io::i32 dx = -1; dx <= 1; ++dx)
                for (io::i32 dz = -1; dz <= 1; ++dz)
                    enqueue_sand_cell(wx + dx, wy + 1, wz + dz, now_ms);

            enqueue_sand_cell(wx, wy - 1, wz, now_ms);
            enqueue_sand_cell(wx - 1, wy - 1, wz, now_ms);
            enqueue_sand_cell(wx + 1, wy - 1, wz, now_ms);
            enqueue_sand_cell(wx, wy - 1, wz - 1, now_ms);
            enqueue_sand_cell(wx, wy - 1, wz + 1, now_ms);
        }

        IO_NODISCARD inline bool dequeue_sand_cell(SandActiveCell& out) noexcept {
            if (!sand_active || stats.sand_active_count == 0u)
                return false;
            out = sand_active[stats.sand_active_head];
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(out.wx, out.wy, out.wz, cc, lx, ly, lz);
            const io::u32 si = find_hot_chunk_sim_slot(cc);
            if (si != 0xFFFFFFFFu && hot_sim[si].used) {
                const io::u32 linear = ge::voxel::chunk_index(lx, ly, lz);
                hot_bit_clear(hot_sim[si].sand_queued, linear);
            }
            stats.sand_active_head = (stats.sand_active_head + 1u) % SAND_ACTIVE_CAP;
            --stats.sand_active_count;
            return true;
        }

        static inline void mark_touched_chunk(ge::voxel::ChunkCoord* coords, io::u32& count,
                                              io::u32 cap,
                                              const ge::voxel::ChunkCoord& coord) noexcept {
            for (io::u32 i = 0; i < count; ++i)
                if (ge::voxel::coord_eq(coords[i], coord))
                    return;
            if (count >= cap) return;
            coords[count++] = coord;
        }

        inline void enqueue_deferred_block_edit(const ge::net::BlockEdit& edit,
                                                bool prefer_unreliable) noexcept {
            if (!deferred_block_edits) return;

            io::u32 scan = deferred_block_edit_count;
            if (scan > BLOCK_EDIT_DEFER_DEDUP_SCAN)
                scan = BLOCK_EDIT_DEFER_DEDUP_SCAN;
            for (io::u32 i = 0u; i < scan; ++i) {
                const io::u32 rel = i + 1u;
                const io::u32 idx = (deferred_block_edit_tail + BLOCK_EDIT_DEFER_CAP - rel) % BLOCK_EDIT_DEFER_CAP;
                DeferredBlockEdit& cur = deferred_block_edits[idx];
                if (cur.edit.wx == edit.wx && cur.edit.wy == edit.wy && cur.edit.wz == edit.wz) {
                    cur.edit = edit;
                    cur.prefer_unreliable = prefer_unreliable;
                    return;
                }
            }

            if (deferred_block_edit_count >= BLOCK_EDIT_DEFER_CAP) {
                deferred_block_edit_head = (deferred_block_edit_head + 1u) % BLOCK_EDIT_DEFER_CAP;
                --deferred_block_edit_count;
            }

            DeferredBlockEdit& dst = deferred_block_edits[deferred_block_edit_tail];
            dst.edit = edit;
            dst.prefer_unreliable = prefer_unreliable;
            deferred_block_edit_tail = (deferred_block_edit_tail + 1u) % BLOCK_EDIT_DEFER_CAP;
            ++deferred_block_edit_count;
        }

        inline void send_world_cell_edit(io::i32 wx, io::i32 wy, io::i32 wz, ge::voxel::BlockId id, io::u16 state,
                                         io::u64 now_ms, bool prefer_unreliable = false) noexcept {
            ge::net::BlockEdit edit{};
            edit.wx = wx;
            edit.wy = wy;
            edit.wz = wz;
            edit.block_id = ge::voxel::block_index(id);
            edit.state = state;
            (void)broadcast_block_edit_ex(edit, now_ms, prefer_unreliable);
        }

        IO_NODISCARD inline bool apply_sand_move(io::i32 src_wx, io::i32 src_wy, io::i32 src_wz,
                                                 io::i32 dst_wx, io::i32 dst_wy, io::i32 dst_wz,
                                                 ge::voxel::BlockId dst_prev_id, io::u16 dst_prev_state,
                                                 ge::voxel::ChunkCoord* touched, io::u32& touched_count,
                                                 io::u64 now_ms, bool send_to_peers) noexcept {
            if (src_wx == dst_wx && src_wy == dst_wy && src_wz == dst_wz)
                return false;

            ge::voxel::BlockId src_now_id = ge::voxel::BlockId::Air;
            io::u16 src_now_state = 0u;
            if (!try_get_loaded_world_state(src_wx, src_wy, src_wz, src_now_id, src_now_state))
                return false;
            if (src_now_id != ge::voxel::BlockId::Sand)
                return false;

            ge::voxel::BlockId dst_now_id = ge::voxel::BlockId::Air;
            io::u16 dst_now_state = 0u;
            if (!try_get_world_state_for_sand(dst_wx, dst_wy, dst_wz, dst_now_id, dst_now_state))
                return false;
            if (dst_now_id != dst_prev_id)
                return false;
            if (!sand_can_move_into(dst_now_id))
                return false;

            ge::voxel::ChunkCoord src_cc{};
            ge::voxel::ChunkCoord dst_cc{};
            io::u32 lx = 0, ly = 0, lz = 0;
            ge::voxel::split_world_coord(src_wx, src_wy, src_wz, src_cc, lx, ly, lz);
            ge::voxel::split_world_coord(dst_wx, dst_wy, dst_wz, dst_cc, lx, ly, lz);
            if (!ensure_world_chunk(src_cc)) return false;
            if (!ensure_world_chunk(dst_cc)) return false;

            if (!world.set_world_state(dst_wx, dst_wy, dst_wz, ge::voxel::BlockId::Sand, 0u, false))
                return false;
            hot_update_flags_cell(dst_wx, dst_wy, dst_wz, ge::voxel::BlockId::Sand, now_ms);
            ge::voxel::BlockId src_next_id = ge::voxel::BlockId::Air;
            io::u16 src_next_state = 0u;
            if (ge::voxel::is_liquid(dst_prev_id)) {
                src_next_id = dst_prev_id;
                src_next_state = dst_prev_state;
            }
            if (!world.set_world_state(src_wx, src_wy, src_wz, src_next_id, src_next_state, false))
                return false;
            hot_update_flags_cell(src_wx, src_wy, src_wz, src_next_id, now_ms);
            enqueue_sand_neighborhood(src_wx, src_wy, src_wz, now_ms);
            enqueue_sand_neighborhood(dst_wx, dst_wy, dst_wz, now_ms);
            enqueue_water_neighborhood(src_wx, src_wy, src_wz, now_ms);
            enqueue_water_neighborhood(dst_wx, dst_wy, dst_wz, now_ms);

            mark_touched_chunk(touched, touched_count, SAND_SAVE_CAP, src_cc);
            mark_touched_chunk(touched, touched_count, SAND_SAVE_CAP, dst_cc);

            if (send_to_peers) {
                send_world_cell_edit(src_wx, src_wy, src_wz, src_next_id, src_next_state, now_ms);
                send_world_cell_edit(dst_wx, dst_wy, dst_wz, ge::voxel::BlockId::Sand, 0u, now_ms);
            }
            return true;
        }

        IO_NODISCARD inline bool apply_sand_vertical_stack(io::i32 wx, io::i32 wy, io::i32 wz,
                                                           ge::voxel::ChunkCoord* touched, io::u32& touched_count,
                                                           SandReservedCell* reserved, io::u32& reserved_count,
                                                           io::u32& moved, io::u64 now_ms, bool send_to_peers) noexcept {
            if (moved >= SAND_MOVE_BUDGET)
                return false;

            ge::voxel::BlockId src_id = ge::voxel::BlockId::Air;
            io::u16 src_state = 0u;
            if (!try_get_loaded_world_state(wx, wy, wz, src_id, src_state))
                return false;
            if (src_id != ge::voxel::BlockId::Sand)
                return false;

            ge::voxel::BlockId below_id = ge::voxel::BlockId::Air;
            io::u16 below_state = 0u;
            if (!try_get_world_state_for_sand(wx, wy - 1, wz, below_id, below_state))
                return false;
            if (sand_can_move_into(below_id)) {
                if (is_reserved_cell(reserved, reserved_count, wx, wy - 1, wz))
                    return false;
                if (!apply_sand_move(wx, wy, wz, wx, wy - 1, wz, below_id, below_state, touched, touched_count, now_ms, send_to_peers))
                    return false;
                reserve_cell(reserved, reserved_count, wx, wy - 1, wz);
                ++moved;
                return true;
            }

            // Diagonal slide (Noita-like): if blocked directly below, try down-diagonal.
            static constexpr io::i32 DIAG[8][2]{
                { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 },
                { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 }
            };
            io::u32 order[8]{ 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u };
            const io::u32 seed = static_cast<io::u32>((wx * 73856093) ^ (wy * 19349663) ^ (wz * 83492791) ^ static_cast<io::i32>(now_ms & 0xFFFFu));
            for (io::u32 i = 0u; i < 8u; ++i) {
                const io::u32 j = (seed + i * 5u) & 7u;
                const io::u32 t = order[i];
                order[i] = order[j];
                order[j] = t;
            }

            for (io::u32 n = 0u; n < 8u; ++n) {
                const io::i32 dx = DIAG[order[n]][0];
                const io::i32 dz = DIAG[order[n]][1];
                const io::i32 tx = wx + dx;
                const io::i32 ty = wy - 1;
                const io::i32 tz = wz + dz;
                if (is_reserved_cell(reserved, reserved_count, tx, ty, tz))
                    continue;

                // Path clearance: diagonal must not "cut through" solid side walls.
                if (dx != 0 && dz != 0) {
                    if (hot_cell_is_collision(wx + dx, wy, wz, now_ms) && hot_cell_is_collision(wx, wy, wz + dz, now_ms))
                        continue;
                } else {
                    if (hot_cell_is_collision(wx + dx, wy, wz + dz, now_ms))
                        continue;
                }

                ge::voxel::BlockId dst_id = ge::voxel::BlockId::Air;
                io::u16 dst_state = 0u;
                if (!try_get_world_state_for_sand(tx, ty, tz, dst_id, dst_state))
                    continue;
                if (!sand_can_move_into(dst_id))
                    continue;
                if (!apply_sand_move(wx, wy, wz, tx, ty, tz, dst_id, dst_state, touched, touched_count, now_ms, send_to_peers))
                    continue;
                reserve_cell(reserved, reserved_count, tx, ty, tz);
                ++moved;
                return true;
            }
            return false;
        }

        inline void process_sand_chunk_desc(ge::voxel::ChunkData& chunk,
                                            io::u32& cursor_desc,
                                            io::u32 scan_cap,
                                            io::u32& scanned,
                                            io::u32& moved,
                                            ge::voxel::ChunkCoord* touched,
                                            io::u32& touched_count,
                                            SandReservedCell* reserved,
                                            io::u32& reserved_count,
                                            io::u64 now_ms,
                                            bool send_to_peers) noexcept {
            if (!chunk.generated || chunk.non_air_count == 0u) return;
            if (cursor_desc >= ge::voxel::CHUNK_VOLUME)
                cursor_desc = 0u;

            io::u32 local_scanned = 0u;
            io::u32 idx = cursor_desc;
            for (; idx < ge::voxel::CHUNK_VOLUME; ++idx) {
                if (scanned >= SAND_SCAN_BUDGET || moved >= SAND_MOVE_BUDGET) break;
                if (local_scanned >= scan_cap) break;
                ++local_scanned;
                ++scanned;

                const io::u32 linear = idx;
                const ge::voxel::BlockState state = chunk.blocks[linear];
                if (state.id != ge::voxel::block_index(ge::voxel::BlockId::Sand))
                    continue;

                const io::u32 lx = linear & 31u;
                const io::u32 lz = (linear >> 5u) & 31u;
                const io::u32 ly = (linear >> 10u) & 31u;
                io::i32 cur_wx = chunk.coord.x * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lx);
                io::i32 cur_wy = chunk.coord.y * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(ly);
                io::i32 cur_wz = chunk.coord.z * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lz);

                for (io::u32 step = 0u; step < SAND_CHAIN_STEPS; ++step) {
                    if (!apply_sand_vertical_stack(cur_wx, cur_wy, cur_wz,
                                                   touched, touched_count,
                                                   reserved, reserved_count,
                                                   moved, now_ms, send_to_peers))
                        break;
                    --cur_wy;
                }
            }

            if (idx >= ge::voxel::CHUNK_VOLUME)
                cursor_desc = 0u;
            else
                cursor_desc = idx;
        }

        inline void simulate_sand(io::u64 now_ms) noexcept {
            if (now_ms < stats.sand_next_step_ms) return;
            stats.sand_next_step_ms = now_ms + SAND_STEP_INTERVAL_MS;
            if (!sand_touched || !sand_reserved || !sand_active) return;
            if (active_peers() == 0u) return;

            ge::voxel::ChunkCoord* touched = sand_touched;
            SandReservedCell* reserved = sand_reserved;
            io::u32 touched_count = 0u;
            io::u32 reserved_count = 0u;
            io::u32 scanned = 0u;
            io::u32 moved = 0u;
            const bool send_to_peers = active_peers() > 0u;

            // If active queue is almost empty, seed it slowly from hot chunks.
            if (stats.sand_active_count < (SAND_SCAN_BUDGET / 2u) && !world.chunks.empty()) {
                const io::usize chunk_count = world.chunks.size();
                io::u32 chunk_idx = static_cast<io::u32>(stats.sand_chunk_cursor % chunk_count);
                io::u32 linear_idx = stats.sand_linear_cursor;
                if (linear_idx >= ge::voxel::CHUNK_VOLUME) linear_idx = 0u;

                io::u32 seed_scan = 0u;
                io::u32 no_hot_cycle = 0u;
                while (seed_scan < SAND_SCAN_BUDGET) {
                    ge::voxel::ChunkData& chunk = world.chunks[chunk_idx];
                    if (!is_chunk_hot_any(chunk.coord)) {
                        ++seed_scan;
                        ++no_hot_cycle;
                        linear_idx = 0u;
                        chunk_idx = (chunk_idx + 1u) % static_cast<io::u32>(chunk_count);
                        if (no_hot_cycle >= static_cast<io::u32>(chunk_count)) break;
                        continue;
                    }
                    no_hot_cycle = 0u;
                    const ge::voxel::BlockState state = chunk.blocks[linear_idx];
                    if (state.id == ge::voxel::block_index(ge::voxel::BlockId::Sand)) {
                        const io::u32 lx = linear_idx & 31u;
                        const io::u32 lz = (linear_idx >> 5u) & 31u;
                        const io::u32 ly = (linear_idx >> 10u) & 31u;
                        const io::i32 wx = chunk.coord.x * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lx);
                        const io::i32 wy = chunk.coord.y * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(ly);
                        const io::i32 wz = chunk.coord.z * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lz);
                        enqueue_sand_cell(wx, wy, wz, now_ms);
                    }
                    ++linear_idx;
                    ++seed_scan;
                    if (linear_idx >= ge::voxel::CHUNK_VOLUME) {
                        linear_idx = 0u;
                        chunk_idx = (chunk_idx + 1u) % static_cast<io::u32>(chunk_count);
                    }
                }
                stats.sand_chunk_cursor = chunk_idx;
                stats.sand_linear_cursor = linear_idx;
            }

            while (scanned < SAND_SCAN_BUDGET && moved < SAND_MOVE_BUDGET) {
                SandActiveCell cell{};
                if (!dequeue_sand_cell(cell))
                    break;
                ++scanned;
                ge::voxel::ChunkCoord cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(cell.wx, cell.wy, cell.wz, cc, lx, ly, lz);
                if (!is_chunk_hot_any(cc))
                    continue;
                (void)apply_sand_vertical_stack(cell.wx, cell.wy, cell.wz,
                                                touched, touched_count,
                                                reserved, reserved_count,
                                                moved, now_ms, send_to_peers);
            }

            for (io::u32 i = 0; i < touched_count; ++i) {
                ge::voxel::ChunkData* c = world.find_chunk(touched[i]);
                if (!c) continue;
                c->generated = true;
                c->dirty_mesh = false;
                c->dirty_neighbors = false;
                (void)ge::voxel::save_chunk_binary(*c);
            }
        }

        IO_NODISCARD static inline io::u16 water_edit_priority(ge::voxel::BlockId id, io::u16 state) noexcept {
            if (is_water_source_active(id, state))
                return 1000u;
            if (id == ge::voxel::BlockId::WaterDark)
                return static_cast<io::u16>(100u + water_flow_level_from_state(state));
            if (id == ge::voxel::BlockId::Air)
                return 0u;
            return 10u;
        }

        IO_NODISCARD static inline io::u32 find_water_edit_slot(const WaterPendingEdit* edits, io::u32 count,
                                                                 io::i32 wx, io::i32 wy, io::i32 wz) noexcept {
            for (io::u32 i = 0u; i < count; ++i)
                if (edits[i].wx == wx && edits[i].wy == wy && edits[i].wz == wz)
                    return i;
            return 0xFFFFFFFFu;
        }

        IO_NODISCARD static inline bool queue_water_edit(WaterPendingEdit* edits,
                                                          io::u32& count,
                                                          io::i32 wx, io::i32 wy, io::i32 wz,
                                                          ge::voxel::BlockId id,
                                                          io::u16 state) noexcept {
            const io::u32 existing = find_water_edit_slot(edits, count, wx, wy, wz);
            if (existing != 0xFFFFFFFFu) {
                const io::u16 cur_prio = water_edit_priority(edits[existing].id, edits[existing].state);
                const io::u16 next_prio = water_edit_priority(id, state);
                if (next_prio >= cur_prio) {
                    edits[existing].id = id;
                    edits[existing].state = state;
                }
                return true;
            }
            if (count >= WATER_EDIT_BUDGET)
                return false;
            edits[count].wx = wx;
            edits[count].wy = wy;
            edits[count].wz = wz;
            edits[count].id = id;
            edits[count].state = state;
            ++count;
            return true;
        }

        inline void enqueue_water_cell(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms = 0u) noexcept {
            if (!water_active) return;
            if (now_ms == 0u) now_ms = io::monotonic_ms();
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            if (!is_chunk_hot_any(cc)) return;
            HotChunkSim* hs = nullptr;
            const io::u32 si = find_hot_chunk_sim_slot(cc);
            if (si != 0xFFFFFFFFu && hot_sim[si].used)
                hs = &hot_sim[si];
            else
                hs = ensure_hot_chunk_sim(cc, now_ms);
            if (!hs) return;
            const io::u32 linear = ge::voxel::chunk_index(lx, ly, lz);
            if (hot_bit_test(hs->water_queued, linear))
                return;
            hot_bit_set(hs->water_queued, linear);
            if (stats.water_active_count >= WATER_ACTIVE_CAP) {
                const WaterActiveCell drop = water_active[stats.water_active_head];
                ge::voxel::ChunkCoord dcc{};
                ge::voxel::split_world_coord(drop.wx, drop.wy, drop.wz, dcc, lx, ly, lz);
                const io::u32 di = find_hot_chunk_sim_slot(dcc);
                if (di != 0xFFFFFFFFu && hot_sim[di].used) {
                    const io::u32 dlin = ge::voxel::chunk_index(lx, ly, lz);
                    hot_bit_clear(hot_sim[di].water_queued, dlin);
                }
                stats.water_active_head = (stats.water_active_head + 1u) % WATER_ACTIVE_CAP;
                --stats.water_active_count;
            }
            water_active[stats.water_active_tail] = WaterActiveCell{ wx, wy, wz };
            stats.water_active_tail = (stats.water_active_tail + 1u) % WATER_ACTIVE_CAP;
            ++stats.water_active_count;
        }

        inline void enqueue_water_neighborhood(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms = 0u) noexcept {
            // Wake all nearby fluid cells so cross-kind interactions (e.g. blood
            // sinking while water redistributes) keep simulating without stalls.
            for (io::i32 dy = -1; dy <= 1; ++dy)
                for (io::i32 dx = -1; dx <= 1; ++dx)
                    for (io::i32 dz = -1; dz <= 1; ++dz)
                        enqueue_water_cell(wx + dx, wy + dy, wz + dz, now_ms);
        }

        IO_NODISCARD inline bool dequeue_water_cell(WaterActiveCell& out) noexcept {
            if (!water_active || stats.water_active_count == 0u)
                return false;
            out = water_active[stats.water_active_head];
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(out.wx, out.wy, out.wz, cc, lx, ly, lz);
            const io::u32 si = find_hot_chunk_sim_slot(cc);
            if (si != 0xFFFFFFFFu && hot_sim[si].used) {
                const io::u32 linear = ge::voxel::chunk_index(lx, ly, lz);
                hot_bit_clear(hot_sim[si].water_queued, linear);
            }
            stats.water_active_head = (stats.water_active_head + 1u) % WATER_ACTIVE_CAP;
            --stats.water_active_count;
            return true;
        }

        IO_NODISCARD inline bool schedule_water_flow_into(WaterPendingEdit* edits,
                                                           io::u32& edit_count,
                                                           io::i32 wx, io::i32 wy, io::i32 wz,
                                                           io::u16 target_level) const noexcept {
            if (target_level == 0u)
                return false;
            if (target_level > WATER_LEVEL_MAX)
                target_level = WATER_LEVEL_MAX;

            ge::voxel::BlockId dst_id = ge::voxel::BlockId::Air;
            io::u16 dst_state = 0u;
            if (!try_get_world_state_for_water(wx, wy, wz, dst_id, dst_state))
                return false;
            if (!water_is_fillable(dst_id))
                return false;
            if (is_water_source_active(dst_id, dst_state))
                return false;

            const io::u16 dst_level = water_level_for_state(dst_id, dst_state);
            if (dst_level >= target_level)
                return false;

            return queue_water_edit(edits, edit_count, wx, wy, wz,
                                    ge::voxel::BlockId::WaterDark,
                                    make_water_flow_state(target_level));
        }

        inline void process_water_chunk_desc(ge::voxel::ChunkData& chunk,
                                             io::u32& cursor_desc,
                                             io::u32 scan_cap,
                                             io::u32& scanned,
                                             WaterPendingEdit* edits,
                                             io::u32& edit_count,
                                             io::u32 phase) noexcept {
            if (!chunk.generated || chunk.non_air_count == 0u) return;
            if (cursor_desc >= ge::voxel::CHUNK_VOLUME)
                cursor_desc = 0u;

            io::u32 local_scanned = 0u;
            io::u32 idx = cursor_desc;
            for (; idx < ge::voxel::CHUNK_VOLUME; ++idx) {
                if (scanned >= WATER_SCAN_BUDGET || edit_count >= WATER_EDIT_BUDGET) break;
                if (local_scanned >= scan_cap) break;
                ++local_scanned;
                ++scanned;

                const ge::voxel::BlockState cell = chunk.blocks[idx];
                const ge::voxel::BlockId src_id = (cell.id < ge::voxel::BLOCK_COUNT)
                    ? static_cast<ge::voxel::BlockId>(cell.id)
                    : ge::voxel::BlockId::Air;
                if (!is_water_block_id(src_id))
                    continue;

                const io::u16 src_level = water_level_for_state(src_id, cell.state);
                const bool src_source = is_water_source_active(src_id, cell.state);
                const io::u32 lx = idx & 31u;
                const io::u32 lz = (idx >> 5u) & 31u;
                const io::u32 ly = (idx >> 10u) & 31u;
                const io::i32 wx = chunk.coord.x * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lx);
                const io::i32 wy = chunk.coord.y * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(ly);
                const io::i32 wz = chunk.coord.z * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lz);

                if (src_level == 0u) {
                    (void)queue_water_edit(edits, edit_count, wx, wy, wz, ge::voxel::BlockId::Air, 0u);
                    continue;
                }
                ge::voxel::BlockId below_id = ge::voxel::BlockId::Air;
                io::u16 below_state = 0u;
                if (!try_get_world_state_for_water(wx, wy - 1, wz, below_id, below_state))
                    continue;
                const bool below_fillable = water_is_fillable(below_id) && !is_water_source_active(below_id, below_state);

                if (phase == 0u) {
                    (void)schedule_water_flow_into(edits, edit_count, wx, wy - 1, wz, src_level);
                    continue;
                }

                if (phase == 1u) {
                    if (!below_fillable) {
                        const io::u16 side_level = (src_level > 0u) ? static_cast<io::u16>(src_level - 1u) : 0u;
                        if (side_level > 0u) {
                            (void)schedule_water_flow_into(edits, edit_count, wx - 1, wy, wz, side_level);
                            (void)schedule_water_flow_into(edits, edit_count, wx + 1, wy, wz, side_level);
                            (void)schedule_water_flow_into(edits, edit_count, wx, wy, wz - 1, side_level);
                            (void)schedule_water_flow_into(edits, edit_count, wx, wy, wz + 1, side_level);
                        }
                    }
                    continue;
                }

                if (src_source)
                    continue;

                io::u16 support_level = 0u;
                ge::voxel::BlockId n_id = ge::voxel::BlockId::Air;
                io::u16 n_state = 0u;
                if (try_get_world_state_for_water(wx, wy + 1, wz, n_id, n_state)) {
                    const io::u16 up_level = water_level_for_state(n_id, n_state);
                    if (up_level > support_level) support_level = up_level;
                }

                static constexpr io::i32 OFFS[4][3]{
                    { -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0, 0, 1 }
                };
                for (io::u32 k = 0u; k < 4u; ++k) {
                    if (!try_get_world_state_for_water(wx + OFFS[k][0], wy + OFFS[k][1], wz + OFFS[k][2], n_id, n_state))
                        continue;
                    io::u16 nh = water_level_for_state(n_id, n_state);
                    if (nh > 0u) --nh;
                    if (nh > support_level) support_level = nh;
                }

                if (support_level == 0u) {
                    (void)queue_water_edit(edits, edit_count, wx, wy, wz, ge::voxel::BlockId::Air, 0u);
                } else if (support_level != src_level || src_id != ge::voxel::BlockId::WaterDark) {
                    (void)queue_water_edit(edits, edit_count, wx, wy, wz,
                                           ge::voxel::BlockId::WaterDark,
                                           make_water_flow_state(support_level));
                }
            }

            if (idx >= ge::voxel::CHUNK_VOLUME)
                cursor_desc = 0u;
            else
                cursor_desc = idx;
        }

        inline void apply_water_edits(WaterPendingEdit* edits,
                                      io::u32 edit_count,
                                      ge::voxel::ChunkCoord* touched,
                                      io::u32& touched_count,
                                      io::u64 now_ms,
                                      bool send_to_peers) noexcept {
            io::u32 applied = 0u;
            for (io::u32 i = 0u; i < edit_count; ++i) {
                const WaterPendingEdit& e = edits[i];
                ge::voxel::ChunkCoord cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(e.wx, e.wy, e.wz, cc, lx, ly, lz);
                ge::voxel::BlockId prev_id = ge::voxel::BlockId::Air;
                io::u16 prev_state = 0u;
                if (!try_get_loaded_world_state(e.wx, e.wy, e.wz, prev_id, prev_state))
                    continue;
                if (prev_id == e.id && prev_state == e.state)
                    continue;
                if (!ensure_world_chunk(cc))
                    continue;
                if (!world.set_world_state(e.wx, e.wy, e.wz, e.id, e.state, false))
                    continue;
                hot_update_flags_cell(e.wx, e.wy, e.wz, e.id, now_ms);
                mark_touched_chunk(touched, touched_count, WATER_SAVE_CAP, cc);
                ++applied;
                enqueue_water_neighborhood(e.wx, e.wy, e.wz, now_ms);
                if (send_to_peers) {
                    const bool fluid = is_water_block_id(e.id) || e.id == ge::voxel::BlockId::Air;
                    send_world_cell_edit(e.wx, e.wy, e.wz, e.id, e.state, now_ms, fluid);
                }
            }

            (void)applied;
        }

