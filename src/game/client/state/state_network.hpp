    inline bool BeginConnectToEndpoint(io::char_view ip, io::u16 port) noexcept {
        if (!net_ready.load()) return false;
        const io::u32 addr_be = io::IP::from_string(ip);
        if (addr_be == 0u || port == 0u) return false;

        net_target_lock.lock();
        net_target_endpoint.addr_be = addr_be;
        net_target_endpoint.port_be = io::h2ns(port);
        net_target_lock.unlock();

        net_connect_wanted.store(1u);
        net_force_handshake.store(1u);
        net_state.store(1u);
        net_ping_ms.store(0u);
        ResetNetTpsHistory();
        ResetNetWorldTimeSync();
        net_last_drop_reason.store(0u);
        net_last_disconnect_reason.store(0u);
        net_received_chunks.store(0u);
        net_last_pos_sent_ms = 0u;
        net_health_lock.lock();
        net_health_pending = false;
        net_health_value = player_hp;
        net_health_damage = 0.f;
        net_health_fall_blocks = 0.f;
        net_health_hunger = player_hunger;
        net_health_flags = player_dead ? ge::net::PLAYER_HEALTH_FLAG_DEAD : 0u;
        net_health_death_reason = player_death_reason;
        net_health_lock.unlock();
        net_pos_lock.lock();
        net_pos_x = camera.position[0];
        net_pos_y = camera.position[1];
        net_pos_z = camera.position[2];
        net_pos_yaw = camera.yaw;
        net_pos_pitch = camera.pitch;
        net_pos_action_flags = 0u;
        net_pos_lock.unlock();
        net_correction_lock.lock();
        net_correction_pending = false;
        net_correction_lock.unlock();
        ResetNetCommandQueue();
        ResetNetIncomingQueue();
        ResetChatLog();
        ResetWorldActors();
        ClearPlayerRoster();
        ClearRemotePlayers();
        ClearRegionStateCache();
        inventory_state = {};
        inventory_flags = 0u;
        inventory_last_snapshot_ms = 0u;
        inventory_open = false;
        ResetWardConfigs();
        ResetHotbarHintTracking();
        net_ping_avg_ms.store(0u);
        net_last_roster_report_ms = 0u;
        net_last_roster_quality = 0xFFu;
        ClearInventoryPick();
        return true;
    }

    inline void StopConnect() noexcept {
        net_connect_wanted.store(0u);
        net_force_handshake.store(0u);
        net_state.store(0u);
        net_ping_ms.store(0u);
        ResetNetTpsHistory();
        ResetNetWorldTimeSync();
        net_session_id.store(0u);
        net_last_pos_sent_ms = 0u;
        net_health_lock.lock();
        net_health_pending = false;
        net_health_damage = 0.f;
        net_health_fall_blocks = 0.f;
        net_health_hunger = player_hunger;
        net_health_flags = player_dead ? ge::net::PLAYER_HEALTH_FLAG_DEAD : 0u;
        net_health_death_reason = player_death_reason;
        net_health_lock.unlock();
        net_correction_lock.lock();
        net_correction_pending = false;
        net_correction_lock.unlock();
        ResetNetCommandQueue();
        ResetNetIncomingQueue();
        ResetChatLog();
        ResetWorldActors();
        ClearPlayerRoster();
        ClearRemotePlayers();
        ClearRegionStateCache();
        inventory_state = {};
        inventory_flags = 0u;
        inventory_last_snapshot_ms = 0u;
        inventory_open = false;
        ResetWardConfigs();
        ResetHotbarHintTracking();
        net_ping_avg_ms.store(0u);
        net_last_roster_report_ms = 0u;
        net_last_roster_quality = 0xFFu;
        ClearInventoryPick();
    }

    inline bool BeginConnectSelectedServer() noexcept {
        if (server_list.empty() || server_selected_index >= server_list.size()) return false;
        const ge::ServerListEntry& e = server_list[server_selected_index];
        const io::char_view ip{ e.ip_utf8 };
        const io::u16 port = (e.port == 0u) ? 25565u : e.port;

        io::StackOut<96> endpoint_text{};
        endpoint_text << ip << ":" << static_cast<io::u32>(port);
        SetSessionText(endpoint_text.view(), io::char_view{ e.name_utf8 });
        session.mode = SessionMode::Multiplayer;

        if (!BeginConnectToEndpoint(ip, port))
            return false;

        screen = ScreenState::Connecting;
        setCursorVisible(true);
        frame.first_mouse_sample = true;
        ClearChunkWorld();
        return true;
    }

    IO_NODISCARD inline io::u32 HashChunkCoord(const ge::voxel::ChunkCoord& coord) const noexcept {
        io::u32 x = static_cast<io::u32>(coord.x);
        io::u32 y = static_cast<io::u32>(coord.y);
        io::u32 z = static_cast<io::u32>(coord.z);
        io::u32 h = 2166136261u;
        h ^= x; h *= 16777619u;
        h ^= y; h *= 16777619u;
        h ^= z; h *= 16777619u;
        h ^= h >> 16u;
        h *= 0x7FEB352Du;
        h ^= h >> 15u;
        h *= 0x846CA68Bu;
        h ^= h >> 16u;
        return h;
    }

    IO_NODISCARD inline io::u32 NextPow2U32(io::u32 v) const noexcept {
        if (v <= 1u) return 1u;
        --v;
        v |= v >> 1u;
        v |= v >> 2u;
        v |= v >> 4u;
        v |= v >> 8u;
        v |= v >> 16u;
        return v + 1u;
    }

    inline void InvalidateChunkSlotLookup() noexcept {
        chunk_slot_lookup_dirty = true;
    }

    inline void ClearChunkSlotLookup() noexcept {
        chunk_slot_lookup.clear();
        chunk_slot_lookup_mask = 0u;
        chunk_slot_lookup_dirty = true;
    }

    IO_NODISCARD inline bool EnsureChunkSlotLookupCapacity(io::u32 wanted_items) noexcept {
        io::u32 cap = static_cast<io::u32>(chunk_slot_lookup.size());
        if (cap != 0u && wanted_items * 2u <= cap)
            return true;

        cap = wanted_items * 2u;
        if (cap < 8u) cap = 8u;
        cap = NextPow2U32(cap);
        if (!chunk_slot_lookup.resize(cap))
            return false;
        for (io::u32 i = 0u; i < cap; ++i) {
            chunk_slot_lookup[i].state = 0u;
            chunk_slot_lookup[i].slot = io::npos;
        }
        chunk_slot_lookup_mask = cap - 1u;
        for (io::usize slot_index = 0; slot_index < voxel_world.chunks.size(); ++slot_index) {
            const ge::voxel::ChunkCoord coord = voxel_world.chunks[slot_index].coord;
            io::u32 pos = HashChunkCoord(coord) & chunk_slot_lookup_mask;
            bool placed = false;
            for (io::u32 probe = 0u; probe < cap; ++probe) {
                ChunkSlotLookupEntry& e = chunk_slot_lookup[pos];
                if (e.state == 0u || ge::voxel::coord_eq(e.coord, coord)) {
                    e.coord = coord;
                    e.slot = slot_index;
                    e.state = 1u;
                    placed = true;
                    break;
                }
                pos = (pos + 1u) & chunk_slot_lookup_mask;
            }
            if (!placed) {
                chunk_slot_lookup_dirty = true;
                return false;
            }
        }
        chunk_slot_lookup_dirty = false;
        return true;
    }

    IO_NODISCARD inline bool RebuildChunkSlotLookup() noexcept {
        chunk_slot_lookup.clear();
        chunk_slot_lookup_mask = 0u;
        if (voxel_world.chunks.empty()) {
            chunk_slot_lookup_dirty = false;
            return true;
        }

        io::u32 cap = static_cast<io::u32>(voxel_world.chunks.size());
        if (cap > 0x3FFFFFFFu) cap = 0x3FFFFFFFu;
        cap *= 2u;
        if (cap < 8u) cap = 8u;
        cap = NextPow2U32(cap);

        if (!chunk_slot_lookup.resize(cap))
            return false;
        for (io::u32 i = 0u; i < cap; ++i) {
            chunk_slot_lookup[i].state = 0u;
            chunk_slot_lookup[i].slot = io::npos;
        }
        chunk_slot_lookup_mask = cap - 1u;

        for (io::usize slot_index = 0; slot_index < voxel_world.chunks.size(); ++slot_index) {
            const ge::voxel::ChunkCoord coord = voxel_world.chunks[slot_index].coord;
            io::u32 pos = HashChunkCoord(coord) & chunk_slot_lookup_mask;
            bool placed = false;
            for (io::u32 probe = 0u; probe < cap; ++probe) {
                ChunkSlotLookupEntry& e = chunk_slot_lookup[pos];
                if (e.state == 0u || ge::voxel::coord_eq(e.coord, coord)) {
                    e.coord = coord;
                    e.slot = slot_index;
                    e.state = 1u;
                    placed = true;
                    break;
                }
                pos = (pos + 1u) & chunk_slot_lookup_mask;
            }
            if (!placed)
                return false;
        }

        chunk_slot_lookup_dirty = false;
        return true;
    }

    IO_NODISCARD inline io::usize LookupChunkSlotByCoord(const ge::voxel::ChunkCoord& coord) const noexcept {
        if (chunk_slot_lookup.empty() || chunk_slot_lookup_mask == 0u)
            return io::npos;
        const io::u32 cap = static_cast<io::u32>(chunk_slot_lookup.size());
        io::u32 pos = HashChunkCoord(coord) & chunk_slot_lookup_mask;
        for (io::u32 probe = 0u; probe < cap; ++probe) {
            const ChunkSlotLookupEntry& e = chunk_slot_lookup[pos];
            if (e.state == 0u) return io::npos;
            if (e.state == 1u && ge::voxel::coord_eq(e.coord, coord)) return e.slot;
            pos = (pos + 1u) & chunk_slot_lookup_mask;
        }
        return io::npos;
    }

    IO_NODISCARD inline bool ChunkSlotLookupInsertOrUpdate(const ge::voxel::ChunkCoord& coord, io::usize slot) noexcept {
        const io::u32 wanted = static_cast<io::u32>(voxel_world.chunks.size() + 1u);
        if (!EnsureChunkSlotLookupCapacity(wanted))
            return false;

        const io::u32 cap = static_cast<io::u32>(chunk_slot_lookup.size());
        io::u32 pos = HashChunkCoord(coord) & chunk_slot_lookup_mask;
        io::u32 first_tomb = static_cast<io::u32>(-1);
        for (io::u32 probe = 0u; probe < cap; ++probe) {
            ChunkSlotLookupEntry& e = chunk_slot_lookup[pos];
            if (e.state == 0u) {
                if (first_tomb != static_cast<io::u32>(-1))
                    pos = first_tomb;
                chunk_slot_lookup[pos].coord = coord;
                chunk_slot_lookup[pos].slot = slot;
                chunk_slot_lookup[pos].state = 1u;
                return true;
            }
            if (e.state == 2u) {
                if (first_tomb == static_cast<io::u32>(-1))
                    first_tomb = pos;
            } else if (ge::voxel::coord_eq(e.coord, coord)) {
                e.slot = slot;
                return true;
            }
            pos = (pos + 1u) & chunk_slot_lookup_mask;
        }
        chunk_slot_lookup_dirty = true;
        return false;
    }

    inline void ChunkSlotLookupErase(const ge::voxel::ChunkCoord& coord) noexcept {
        if (chunk_slot_lookup.empty() || chunk_slot_lookup_mask == 0u)
            return;
        const io::u32 cap = static_cast<io::u32>(chunk_slot_lookup.size());
        io::u32 pos = HashChunkCoord(coord) & chunk_slot_lookup_mask;
        for (io::u32 probe = 0u; probe < cap; ++probe) {
            ChunkSlotLookupEntry& e = chunk_slot_lookup[pos];
            if (e.state == 0u)
                return;
            if (e.state == 1u && ge::voxel::coord_eq(e.coord, coord)) {
                e.state = 2u;
                e.slot = io::npos;
                return;
            }
            pos = (pos + 1u) & chunk_slot_lookup_mask;
        }
    }

    IO_NODISCARD inline io::usize FindChunkSlotByCoord(const ge::voxel::ChunkCoord& coord) noexcept {
        if (chunk_slot_lookup_dirty)
            (void)RebuildChunkSlotLookup();

        const io::usize idx = LookupChunkSlotByCoord(coord);
        if (idx != io::npos && idx < voxel_world.chunks.size() &&
            ge::voxel::coord_eq(voxel_world.chunks[idx].coord, coord))
            return idx;

        for (io::usize i = 0; i < voxel_world.chunks.size(); ++i)
            if (ge::voxel::coord_eq(voxel_world.chunks[i].coord, coord)) {
                (void)ChunkSlotLookupInsertOrUpdate(coord, i);
                return i;
            }
        return io::npos;
    }

    IO_NODISCARD inline io::usize EnsureChunkSlotByCoord(const ge::voxel::ChunkCoord& coord) noexcept {
        const io::usize existing = FindChunkSlotByCoord(coord);
        if (existing != io::npos)
            return existing;

        ge::voxel::ChunkData* added = voxel_world.ensure_chunk(coord);
        if (!added) {
            const ge::voxel::ChunkCoord center = chunk_center_valid ? chunk_center_coord : CameraChunkCoord();
            if (voxel_world.max_chunks > 0u && voxel_world.chunks.size() >= voxel_world.max_chunks) {
                io::usize victim = io::npos;
                io::i32 best_score = -1;
                for (io::usize i = 0; i < voxel_world.chunks.size(); ++i) {
                    if (HasActiveChunkJobForIndex(i))
                        continue;
                    const ge::voxel::ChunkCoord c = voxel_world.chunks[i].coord;
                    const io::i32 dx = c.x - center.x;
                    const io::i32 dy = c.y - center.y;
                    const io::i32 dz = c.z - center.z;
                    io::i32 score = dx * dx + dy * dy + dz * dz;
                    if (!CoordInRequestBounds(c, center))
                        score += 1 << 29;
                    if (victim == io::npos || score > best_score) {
                        victim = i;
                        best_score = score;
                    }
                }
                if (victim == io::npos)
                    return io::npos;
                const ge::voxel::ChunkCoord victim_coord = voxel_world.chunks[victim].coord;
                RemoveInflightChunkRequest(victim_coord);
                RegenerateChunkSlot(victim, coord);
                RemoveInflightChunkRequest(coord);
                return victim;
            }
            if (!added)
                return io::npos;
        }

        const io::usize need = voxel_world.chunks.size();
        if (chunk_meshes.size() < need) {
            if (!chunk_meshes.resize(need)) {
                (void)voxel_world.remove_chunk(coord);
                InvalidateChunkSlotLookup();
                return io::npos;
            }
        }
        const io::usize idx = need - 1u;
        ChunkRenderMesh& gpu = chunk_meshes[idx];
        gpu.coord = coord;
        gpu.index_count = 0u;
        gpu.built_version = 0u;
        gpu.queued = false;
        gpu.uploaded = false;
        if (!ChunkSlotLookupInsertOrUpdate(coord, idx))
            InvalidateChunkSlotLookup();
        return idx;
    }

    IO_NODISCARD inline bool ApplyLocalBlockEdit(io::i32 wx, io::i32 wy, io::i32 wz,
                                                 ge::voxel::BlockId block_id,
                                                 io::u16 block_state,
                                                 bool create_missing_chunk) noexcept {
        if (!chunk_world_ready) return false;
        return voxel_world.set_world_state(wx, wy, wz, block_id, block_state, create_missing_chunk);
    }

    inline void ApplyIncomingBlockEdit(const ge::net::BlockEdit& edit) noexcept {
        ge::voxel::BlockId block_id = ge::voxel::BlockId::Air;
        if (edit.block_id < ge::voxel::BLOCK_COUNT)
            block_id = static_cast<ge::voxel::BlockId>(edit.block_id);
        UpdateSandLerpFromBlockEdit(edit.wx, edit.wy, edit.wz, block_id);
        (void)ApplyLocalBlockEdit(edit.wx, edit.wy, edit.wz, block_id, edit.state, /*create_missing_chunk=*/false);
    }

    inline void PumpIncomingChunks() noexcept {
        if (!net_incoming_ring) return;
        io::u32 skipped_chunks = 0u;

        for (;;) {
            net_incoming_lock.lock();
            if (net_incoming_head >= NET_INCOMING_CAP || net_incoming_tail >= NET_INCOMING_CAP || net_incoming_count > NET_INCOMING_CAP) {
                net_incoming_head = 0u;
                net_incoming_tail = 0u;
                net_incoming_count = 0u;
                net_incoming_lock.unlock();
                break;
            }
            if (net_incoming_count == 0u) {
                net_incoming_lock.unlock();
                break;
            }

            const io::u32 ring_head = net_incoming_head;
            const NetIncomingType item_type = net_incoming_ring[ring_head].type;

            if (item_type == NetIncomingType::Chunk) {
                if (!chunk_world_ready) {
                    if (net_incoming_count <= 1u || skipped_chunks >= net_incoming_count) {
                        net_incoming_lock.unlock();
                        break;
                    }
                    const io::u32 old_head = net_incoming_head;
                    const io::u32 old_tail = net_incoming_tail;
                    net_incoming_ring[old_tail] = net_incoming_ring[old_head];
                    net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                    net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
                    ++skipped_chunks;
                    net_incoming_lock.unlock();
                    continue;
                }
                const ge::voxel::ChunkCoord coord = net_incoming_ring[ring_head].chunk.coord;
                const ge::voxel::ChunkCoord center = chunk_center_valid ? chunk_center_coord : CameraChunkCoord();
                if (!CoordInViewBounds(coord, center)) {
                    RemoveInflightChunkRequest(coord);
                    net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                    --net_incoming_count;
                    net_incoming_lock.unlock();
                    skipped_chunks = 0u;
                    continue;
                }
                const io::usize slot = FindChunkSlotByCoord(coord);
                if (slot != io::npos && HasActiveChunkJobForIndex(slot)) {
                    if (net_incoming_count <= 1u || skipped_chunks >= net_incoming_count) {
                        net_incoming_lock.unlock();
                        break;
                    }
                    const io::u32 old_head = net_incoming_head;
                    const io::u32 old_tail = net_incoming_tail;
                    net_incoming_ring[old_tail] = net_incoming_ring[old_head];
                    net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                    net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
                    ++skipped_chunks;
                    net_incoming_lock.unlock();
                    continue;
                }
                bool applied = false;
                const io::usize dst_slot = (slot != io::npos) ? slot : EnsureChunkSlotByCoord(coord);
                if (dst_slot != io::npos && dst_slot < voxel_world.chunks.size() && dst_slot < chunk_meshes.size()) {
                    CopyChunkData(voxel_world.chunks[dst_slot], net_incoming_ring[ring_head].chunk);
                    voxel_world.chunks[dst_slot].generated = true;
                    voxel_world.chunks[dst_slot].dirty_mesh = true;
                    voxel_world.chunks[dst_slot].dirty_neighbors = true;
                    InvalidateChunkMesh(chunk_meshes[dst_slot], coord);
                    RemoveInflightChunkRequest(coord);
                    applied = true;
                }
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
                net_incoming_lock.unlock();
                skipped_chunks = 0u;
                if (!applied) continue;
                TouchNeighborsOf(coord);
                ++chunk_net_received;
                continue;
            }

            if (item_type == NetIncomingType::Chat) {
                const ge::net::ChatLine item_chat = net_incoming_ring[ring_head].chat;
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
                net_incoming_lock.unlock();
                skipped_chunks = 0u;
                PushChatLine(item_chat);
                continue;
            }

            if (item_type == NetIncomingType::WorldActor) {
                const ge::net::WorldActorSample item_actor = net_incoming_ring[ring_head].actor;
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
                net_incoming_lock.unlock();
                skipped_chunks = 0u;
                ApplyIncomingWorldActor(item_actor);
                continue;
            }

            if (item_type == NetIncomingType::InventoryState) {
                const ge::net::InventoryStateSample item_inventory = net_incoming_ring[ring_head].inventory;
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
                net_incoming_lock.unlock();
                skipped_chunks = 0u;
                inventory_state = item_inventory.inventory;
                inventory_flags = item_inventory.flags;
                inventory_last_snapshot_ms = io::monotonic_ms();
                UpdateHotbarHintTracking();
                continue;
            }

            if (item_type == NetIncomingType::WardConfigState) {
                const ge::net::WardConfigStateSample item_cfg = net_incoming_ring[ring_head].ward_config;
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
                net_incoming_lock.unlock();
                skipped_chunks = 0u;
                ApplyIncomingWardConfigState(item_cfg);
                continue;
            }

            if (item_type == NetIncomingType::RegionState) {
                const ge::net::RegionStateSample item_region = net_incoming_ring[ring_head].region_state;
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
                net_incoming_lock.unlock();
                skipped_chunks = 0u;
                ApplyIncomingRegionState(item_region);
                continue;
            }

            if (item_type == NetIncomingType::BlockEdit && !chunk_world_ready) {
                net_incoming_lock.unlock();
                break;
            }

            const ge::net::BlockEdit item_edit = net_incoming_ring[ring_head].edit;
            net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
            --net_incoming_count;
            net_incoming_lock.unlock();
            skipped_chunks = 0u;

            if (item_type == NetIncomingType::BlockEdit)
                ApplyIncomingBlockEdit(item_edit);
        }
    }

    IO_NODISCARD inline bool HasActiveChunkJobs() const noexcept {
        if (!chunk_job_slots) return false;
        for (io::u32 i = 0; i < chunk_job_slots_active; ++i) {
            const io::u32 s = chunk_job_slots[i].state.load();
            if (s == static_cast<io::u32>(ChunkJobState::Queued) ||
                s == static_cast<io::u32>(ChunkJobState::Building))
                return true;
        }
        return false;
    }

    IO_NODISCARD inline bool HasActiveChunkJobForIndex(io::usize world_chunk_index) const noexcept {
        if (!chunk_job_slots) return false;
        for (io::u32 i = 0; i < chunk_job_slots_active; ++i) {
            const ChunkMeshJobSlot& slot = chunk_job_slots[i];
            const io::u32 s = slot.state.load();
            if (s != static_cast<io::u32>(ChunkJobState::Queued) &&
                s != static_cast<io::u32>(ChunkJobState::Building))
                continue;
            if (slot.world_chunk_index == world_chunk_index)
                return true;
        }
        return false;
    }

    inline void QueueMissingChunkRequests() noexcept {
        if (!chunk_world_ready || chunk_request_virtual_count == 0u) return;
        if (net_state.load() != 2u) return;
        if (player_dead) {
            chunk_requests_inflight.clear();
            ClearInflightLookup();
            PurgeNetChunkRequestCommands();
            return;
        }
        static constexpr bool SERVER_AUTHORITATIVE_CHUNK_STREAM = false;
        if (SERVER_AUTHORITATIVE_CHUNK_STREAM) {
            chunk_requests_inflight.clear();
            ClearInflightLookup();
            return;
        }
        if (!EnsureChunkRequestOffsets()) return;

        static constexpr io::u64 REQUEST_RESEND_MS = 3000u;
        static constexpr io::u32 REQUEST_RESEND_PER_FRAME_MAX = 6u;
        static constexpr io::u32 REQUEST_SCAN_PER_PICK_BASE = 24u;

        const io::u64 now_ms = io::monotonic_ms();
        const ge::voxel::ChunkCoord center = chunk_center_valid ? chunk_center_coord : CameraChunkCoord();
        const lm::vec3 vel = move_velocity;
        const io::u32 requests_per_frame_max = ge::client::chunk::ComputeRequestsPerFrame(render_distance_chunks);
        const io::u32 outstanding_limit = ge::client::chunk::ComputeOutstandingLimit(chunk_request_virtual_count, voxel_world.max_chunks);
        io::u32 queued = 0u;
        io::u32 resent = 0u;
        const io::u32 virtual_count_u32 = static_cast<io::u32>(chunk_request_offsets.size());
        if (virtual_count_u32 == 0u) return;
        const auto abs_i32_local = [](io::i32 v) noexcept -> io::i32 { return v < 0 ? -v : v; };
        const io::i32 max_phase = static_cast<io::i32>(render_distance_chunks);
        if (chunk_request_radius_phase < 0 || chunk_request_radius_phase > max_phase)
            chunk_request_radius_phase = 0;
        bool phase_has_missing = false;
        for (io::u32 i = 0u; i < virtual_count_u32; ++i) {
            const ge::voxel::ChunkCoord offs = chunk_request_offsets[i];
            const io::i32 ax = abs_i32_local(offs.x);
            const io::i32 az = abs_i32_local(offs.z);
            const io::i32 ring = (ax > az) ? ax : az;
            if (ring > chunk_request_radius_phase) continue;
            const ge::voxel::ChunkCoord coord{
                center.x + offs.x,
                center.y + offs.y,
                center.z + offs.z
            };
            if (!CoordInRequestBounds(coord, center))
                continue;
            if (FindChunkSlotByCoord(coord) != io::npos)
                continue;
            phase_has_missing = true;
            break;
        }
        if (!phase_has_missing && chunk_request_radius_phase < max_phase)
            ++chunk_request_radius_phase;
        io::u32 request_scan_per_pick = REQUEST_SCAN_PER_PICK_BASE;
        if (render_distance_chunks > 6u)
            request_scan_per_pick += (render_distance_chunks - 6u) * 2u;
        if (request_scan_per_pick > 64u)
            request_scan_per_pick = 64u;

        while (queued < requests_per_frame_max) {
            const bool can_send_new = chunk_requests_inflight.size() < outstanding_limit;
            const bool can_resend = resent < REQUEST_RESEND_PER_FRAME_MAX;
            if (!can_send_new && !can_resend)
                break;

            ge::voxel::ChunkCoord best_coord{};
            io::usize best_pending_i = io::npos;
            float best_score = 0.f;
            bool best_valid = false;
            bool best_pending = false;

            for (io::u32 scan = 0u; scan < request_scan_per_pick; ++scan) {
                const io::u32 cursor = chunk_request_scan_cursor;
                chunk_request_scan_cursor = (cursor + 1u >= virtual_count_u32) ? 0u : (cursor + 1u);
                const io::u32 idx_u32 = cursor;
                const io::usize idx = static_cast<io::usize>(idx_u32);
                const ge::voxel::ChunkCoord offs = chunk_request_offsets[idx];
                const io::i32 ax = abs_i32_local(offs.x);
                const io::i32 az = abs_i32_local(offs.z);
                const io::i32 ring = (ax > az) ? ax : az;
                if (ring > chunk_request_radius_phase)
                    continue;
                const ge::voxel::ChunkCoord coord{
                    center.x + offs.x,
                    center.y + offs.y,
                    center.z + offs.z
                };
                if (!CoordInRequestBounds(coord, center))
                    continue;

                if (FindChunkSlotByCoord(coord) != io::npos) {
                    RemoveInflightChunkRequest(coord);
                    continue;
                }

                const io::usize pending_i = FindInflightChunkRequestIndex(coord);
                const bool pending = (pending_i != io::npos);
                if (pending) {
                    if (!can_resend) continue;
                    const io::u64 sent_ms = chunk_requests_inflight[pending_i].sent_ms;
                    if (now_ms < sent_ms + REQUEST_RESEND_MS) continue;
                } else if (!can_send_new) {
                    continue;
                }

                float score = ChunkRequestPriority(coord, center, vel);
                if (pending) score += 0.35f;
                if (!best_valid || score < best_score) {
                    best_valid = true;
                    best_score = score;
                    best_coord = coord;
                    best_pending_i = pending_i;
                    best_pending = pending;
                }
            }

            if (!best_valid) break;

            if (!EnqueueNetChunkRequest(best_coord, 0u))
                break;
            if (best_pending) {
                if (best_pending_i < chunk_requests_inflight.size())
                    chunk_requests_inflight[best_pending_i].sent_ms = now_ms;
                ++resent;
            } else {
                ChunkRequestInflight inflight{};
                inflight.coord = best_coord;
                inflight.sent_ms = now_ms;
                if (!chunk_requests_inflight.push_back(inflight))
                    break;
                const io::usize new_index = chunk_requests_inflight.size() - 1u;
                if (!InflightLookupInsertOrUpdate(best_coord, new_index))
                    InvalidateInflightLookup();
            }
            ++queued;
        }
    }

    static void NetThreadEntry(void* arg) noexcept {
        Window* self = reinterpret_cast<Window*>(arg);
        if (!self) return;
        self->RunNetworkLoop();
    }

    inline void RunNetworkLoop() noexcept {
        if (!net_loop) return;
        io::UdpCallbacks cb{};
        cb.ud = this;
        cb.on_packet = &Window::NetOnPacket;
        cb.on_established = &Window::NetOnEstablished;
        cb.on_drop = &Window::NetOnDrop;
        cb.on_disconnect = &Window::NetOnDisconnect;
        cb.on_tick = &Window::NetOnTick;
        net_loop->run_udp(net_udp, cb, net_recv_buf, static_cast<int>(NET_RECV_BUF_CAP));
    }

    static void NetOnPacket(void* ud, io::Endpoint from, io::u8 type, io::UdpChan chan, io::byte_view payload) noexcept {
        (void)from;
        (void)chan;
        Window* self = reinterpret_cast<Window*>(ud);
        if (!self) return;
        self->OnNetPacket(type, payload);
    }

    inline void OnNetPacket(io::u8 type, io::byte_view payload) noexcept {
        if (type == ge::net::PK_S2C_PONG) {
            ge::net::S2C_Pong pong{};
            if (!ReadPayloadExact(payload, pong)) return;
            const io::u32 now_ms = static_cast<io::u32>(io::monotonic_ms() - net_boot_ms);
            const io::u32 sent_ms = io::n2hl(pong.client_ms_be);
            const io::u32 ping_ms = now_ms >= sent_ms ? (now_ms - sent_ms) : 0u;
            net_ping_ms.store(ping_ms);
            const io::u32 prev_avg = net_ping_avg_ms.load();
            const io::u32 next_avg = (prev_avg == 0u) ? ping_ms : static_cast<io::u32>((prev_avg * 7u + ping_ms) / 8u);
            net_ping_avg_ms.store(next_avg);
            UpdatePlayerRosterQualityByName(io::char_view{ player_name_utf8, player_name_len },
                                            ge::net::signal_quality_from_ping_ms(ping_ms));
            return;
        }

        if (type == ge::net::PK_S2C_SERVER_TPS) {
            ge::net::S2C_ServerTps wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            const ge::net::ServerTpsSample sample = ge::net::decode_s2c_server_tps(wire);
            PushNetTpsSample(sample.tps_x100);
            return;
        }

        if (type == ge::net::PK_S2C_WORLD_TIME) {
            ge::net::S2C_WorldTime wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            PushNetWorldTimeSample(ge::net::decode_s2c_world_time(wire));
            return;
        }

        if (type == ge::net::PK_S2C_PLAYER_HEALTH) {
            ge::net::S2C_PlayerHealth wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            const ge::net::PlayerHealthSample sample = ge::net::decode_s2c_player_health(wire);
            net_health_lock.lock();
            net_health_value = static_cast<float>(sample.hp);
            net_health_damage = sample.damage;
            net_health_fall_blocks = sample.fall_blocks;
            net_health_hunger = sample.hunger;
            net_health_flags = sample.flags;
            net_health_death_reason = sample.death_reason;
            net_health_pending = true;
            net_health_lock.unlock();
            return;
        }

        if (type == ge::net::PK_S2C_PLAYER_POSITION) {
            ge::net::S2C_PlayerPosition wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            const ge::net::ServerPlayerPosition sample = ge::net::decode_server_player_position(wire);
            if ((sample.flags & ge::net::PLAYER_POS_FLAG_CORRECTION) != 0u) {
                net_correction_lock.lock();
                net_correction_x = sample.x;
                net_correction_y = sample.y;
                net_correction_z = sample.z;
                net_correction_pending = true;
                net_correction_lock.unlock();
            }
            return;
        }

        if (type == ge::net::PK_S2C_CHUNK_BEGIN) {
            ge::net::S2C_ChunkBegin wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            if (net_chunk_assembly)
                (void)net_chunk_assembly->begin(ge::net::decode_chunk_begin(wire));
            return;
        }

        if (type == ge::net::PK_S2C_CHUNK_PART) {
            if (payload.size() < sizeof(ge::net::S2C_ChunkPartHeader)) return;
            ge::net::S2C_ChunkPartHeader wire{};
            for (io::usize i = 0; i < sizeof(wire); ++i)
                reinterpret_cast<io::u8*>(&wire)[i] = payload[i];
            const ge::net::ChunkPart part = ge::net::decode_chunk_part(wire);
            const io::byte_view bytes = payload.slice(sizeof(wire), payload.size() - sizeof(wire));
            if (net_chunk_assembly)
                (void)net_chunk_assembly->add_part(part, bytes);
            return;
        }

        if (type == ge::net::PK_S2C_CHUNK_END) {
            ge::net::S2C_ChunkEnd wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            const ge::net::ChunkEnd end = ge::net::decode_chunk_end(wire);
            if (net_chunk_assembly && net_chunk_assembly->end(end) && EnqueueIncomingChunk(net_chunk_assembly->chunk)) {
                net_received_chunks.fetch_add(1u);
                ge::net::ChunkAck ack{};
                ack.request_id = end.request_id;
                ack.coord = net_chunk_assembly->chunk.coord;
                ge::net::C2S_ChunkAck ack_wire{};
                ge::net::encode_chunk_ack(ack, ack_wire);
                if (net_loop)
                    (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_CHUNK_ACK, io::UdpChan::Reliable,
                                                 io::byte_view{ reinterpret_cast<const io::u8*>(&ack_wire), sizeof(ack_wire) }, io::monotonic_ms());
            }
            if (net_chunk_assembly)
                net_chunk_assembly->reset();
            return;
        }

        if (type == ge::net::PK_S2C_BLOCK_EDIT) {
            ge::net::S2C_BlockEdit wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            (void)EnqueueIncomingBlockEdit(ge::net::decode_s2c_block_edit(wire));
            return;
        }

        if (type == ge::net::PK_S2C_WORLD_ACTOR) {
            ge::net::S2C_WorldActor wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            (void)EnqueueIncomingWorldActor(ge::net::decode_s2c_world_actor(wire));
            return;
        }

        if (type == ge::net::PK_S2C_CHAT) {
            ge::net::S2C_Chat wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            (void)EnqueueIncomingChat(ge::net::decode_s2c_chat(wire));
            return;
        }

        if (type == ge::net::PK_S2C_INVENTORY_STATE) {
            ge::net::S2C_InventoryState wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            (void)EnqueueIncomingInventoryState(ge::net::decode_s2c_inventory_state(wire));
            return;
        }

        if (type == ge::net::PK_S2C_WARD_CONFIG_STATE) {
            ge::net::S2C_WardConfigState wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            (void)EnqueueIncomingWardConfigState(ge::net::decode_s2c_ward_config_state(wire));
            return;
        }

        if (type == ge::net::PK_S2C_PLAYER_ROSTER_PAGE) {
            if (payload.size() < sizeof(ge::net::S2C_PlayerRosterPage)) return;
            ge::net::S2C_PlayerRosterPage header{};
            for (io::usize i = 0u; i < sizeof(header); ++i)
                reinterpret_cast<io::u8*>(&header)[i] = payload[i];
            ge::net::PlayerRosterPage page = ge::net::decode_s2c_player_roster_page_header(header);
            const io::usize expected = sizeof(ge::net::S2C_PlayerRosterPage) +
                static_cast<io::usize>(page.count) * sizeof(ge::net::PlayerRosterEntryWire);
            if (payload.size() < expected) return;
            io::usize cursor = sizeof(ge::net::S2C_PlayerRosterPage);
            for (io::u32 i = 0u; i < page.count; ++i) {
                ge::net::PlayerRosterEntryWire wire_entry{};
                for (io::usize b = 0u; b < sizeof(wire_entry); ++b)
                    reinterpret_cast<io::u8*>(&wire_entry)[b] = payload[cursor + b];
                cursor += sizeof(wire_entry);
                page.entries[i] = ge::net::decode_player_roster_entry(wire_entry);
            }
            ApplyPlayerRosterPage(page);
            return;
        }

        if (type == ge::net::PK_S2C_PLAYER_ROSTER_ADD) {
            ge::net::S2C_PlayerRosterAdd wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            UpsertPlayerRosterEntry(ge::net::decode_s2c_player_roster_add(wire));
            return;
        }

        if (type == ge::net::PK_S2C_PLAYER_ROSTER_REMOVE) {
            ge::net::S2C_PlayerRosterRemove wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            const ge::net::PlayerRosterRemove sample = ge::net::decode_s2c_player_roster_remove(wire);
            RemovePlayerRosterByServerIndex(sample.server_index);
            RemoveRemotePlayerByServerIndex(sample.server_index);
            return;
        }

        if (type == ge::net::PK_S2C_REMOTE_PLAYER_POSE) {
            ge::net::S2C_RemotePlayerPose wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            ApplyRemotePlayerPose(ge::net::decode_s2c_remote_player_pose(wire));
            return;
        }

        if (type == ge::net::PK_S2C_REGION_STATE) {
            ge::net::S2C_RegionState wire{};
            if (!ReadPayloadExact(payload, wire)) return;
            (void)EnqueueIncomingRegionState(ge::net::decode_s2c_region_state(wire));
            return;
        }
    }

    static void NetOnEstablished(void* ud, io::Endpoint peer, io::u32 session_id) noexcept {
        Window* self = reinterpret_cast<Window*>(ud);
        if (!self) return;
        self->net_connected_endpoint = peer;
        self->net_session_id.store(session_id);
        self->net_state.store(2u);
        self->ResetNetTpsHistory();
        self->ResetNetWorldTimeSync();
        self->net_last_drop_reason.store(0u);
        self->net_last_disconnect_reason.store(0u);
        self->net_force_handshake.store(0u);
        self->ClearPlayerRoster();
        self->ClearRemotePlayers();
        self->ClearRegionStateCache();
        self->net_ping_avg_ms.store(0u);
        self->net_last_roster_report_ms = 0u;
        self->net_last_roster_quality = 0xFFu;
        self->net_last_melee_sent_ms = 0u;
        self->SendPlayerRosterSelfUpdate(io::monotonic_ms());
        self->RequestPlayerRoster(0u, static_cast<io::u16>(ge::net::PLAYER_ROSTER_CLIENT_CAP), io::monotonic_ms());
        ge::net::InventoryAction inv_bootstrap{};
        inv_bootstrap.action = ge::net::INVENTORY_ACTION_SELECT_HOTBAR;
        inv_bootstrap.src_region = ge::item::SlotRegion::Hotbar;
        inv_bootstrap.src_index = (self->inventory_state.selected_hotbar < ge::item::HOTBAR_SLOT_COUNT)
            ? self->inventory_state.selected_hotbar : 0u;
        (void)self->EnqueueNetInventoryAction(inv_bootstrap);
        self->PushSystemChat("Connected");
    }

    static void NetOnDrop(void* ud, io::Endpoint from, io::Error err, io::DropReason why) noexcept {
        (void)from;
        (void)err;
        Window* self = reinterpret_cast<Window*>(ud);
        if (!self) return;
        self->net_last_drop_reason.store(static_cast<io::u32>(why));
        self->ResetNetTpsHistory();
        self->ResetNetWorldTimeSync();
        self->ClearPlayerRoster();
        self->ClearRemotePlayers();
        self->ClearRegionStateCache();
        self->net_ping_avg_ms.store(0u);
        self->net_last_roster_report_ms = 0u;
        self->net_last_roster_quality = 0xFFu;
        self->net_last_melee_sent_ms = 0u;
        if (self->net_connect_wanted.load()) self->net_state.store(1u);
        else self->net_state.store(0u);
        self->PushSystemChat("Connection dropped");
    }

    static void NetOnDisconnect(void* ud, io::Endpoint peer, io::u32 session_id, io::DisconnectReason why) noexcept {
        (void)peer;
        (void)session_id;
        Window* self = reinterpret_cast<Window*>(ud);
        if (!self) return;
        self->net_last_disconnect_reason.store(static_cast<io::u32>(why));
        self->net_session_id.store(0u);
        self->net_ping_ms.store(0u);
        self->net_ping_avg_ms.store(0u);
        self->ResetNetTpsHistory();
        self->ResetNetWorldTimeSync();
        self->ClearPlayerRoster();
        self->ClearRemotePlayers();
        self->ClearRegionStateCache();
        self->net_last_roster_report_ms = 0u;
        self->net_last_roster_quality = 0xFFu;
        self->net_last_melee_sent_ms = 0u;
        if (self->net_connect_wanted.load()) self->net_state.store(1u);
        else self->net_state.store(0u);
        self->PushSystemChat("Disconnected");
    }

    static void NetOnTick(void* ud, io::u64 now_ms) noexcept {
        Window* self = reinterpret_cast<Window*>(ud);
        if (!self) return;
        self->OnNetTick(now_ms);
    }

    inline void OnNetTick(io::u64 now_ms) noexcept {
        if (!net_connect_wanted.load()) {
            if (net_state.load() == 2u) {
                if (net_loop)
                    (void)net_loop->disconnect_peer(net_connected_endpoint, io::DisconnectReason::LocalReset, now_ms);
                net_state.store(0u);
            }
            return;
        }

        if (net_force_handshake.exchange(0u) != 0u)
        {
            if (net_chunk_assembly)
                net_chunk_assembly->reset();
            net_next_handshake_ms = 0u;
        }

        io::Endpoint target{};
        net_target_lock.lock();
        target = net_target_endpoint;
        net_target_lock.unlock();

        if (net_state.load() == 2u && !io::endpoint_eq(target, net_connected_endpoint)) {
            if (net_loop)
                (void)net_loop->disconnect_peer(net_connected_endpoint, io::DisconnectReason::LocalReset, now_ms);
            net_state.store(1u);
            net_next_handshake_ms = 0u;
            return;
        }

        if (net_state.load() != 2u && now_ms >= net_next_handshake_ms) {
            if (net_loop && target.addr_be != 0u && target.port_be != 0u) {
                (void)net_loop->get_peer_create(target);
                if (net_loop->start_client_handshake(target, io::DEFAULT_MTU, io::FEATURE_COOKIE, now_ms)) {
                    net_handshake_attempts.fetch_add(1u);
                    net_state.store(1u);
                }
            }
            net_next_handshake_ms = now_ms + 1000u;
        }

        if (net_state.load() != 2u) return;

        if (now_ms - net_last_ping_sent_ms >= 200u) {
            ge::net::C2S_Ping ping{};
            ping.client_ms_be = io::h2nl(static_cast<io::u32>(now_ms - net_boot_ms));
            if (net_loop)
                (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_PING, io::UdpChan::Unreliable,
                                             io::byte_view{ reinterpret_cast<const io::u8*>(&ping), sizeof(ping) }, now_ms);
            net_last_ping_sent_ms = now_ms;
        }

        SendPlayerRosterSelfUpdate(now_ms);

        if (now_ms - net_last_pos_sent_ms >= 50u) {
            float x = 0.f, y = 0.f, z = 0.f;
            float yaw = 0.f, pitch = 0.f;
            io::u8 action_flags = 0u;
            net_pos_lock.lock();
            x = net_pos_x;
            y = net_pos_y;
            z = net_pos_z;
            yaw = net_pos_yaw;
            pitch = net_pos_pitch;
            action_flags = net_pos_action_flags;
            net_pos_lock.unlock();

            ge::net::PlayerPositionSample sample{};
            sample.client_ms = static_cast<io::u32>(now_ms - net_boot_ms);
            sample.x = x;
            sample.y = y;
            sample.z = z;
            sample.yaw = yaw;
            sample.pitch = pitch;
            sample.action_flags = action_flags;
            ge::net::C2S_PlayerPosition wire{};
            ge::net::encode_player_position(sample, wire);
            if (net_loop)
                (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_PLAYER_POSITION, io::UdpChan::Unreliable,
                                             io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            net_last_pos_sent_ms = now_ms;
        }

        NetChunkCmd cmd{};
        io::u32 send_budget = 64u;
        while (send_budget > 0u && DequeueNetChunkRequest(cmd)) {
            if (cmd.type == NetCmdType::RequestChunk) {
                ge::net::ChunkRequest req{};
                req.request_id = net_next_request_id++;
                if (net_next_request_id == 0u) net_next_request_id = 1u;
                req.cx = cmd.coord.x;
                req.cy = cmd.coord.y;
                req.cz = cmd.coord.z;
                req.lod = cmd.lod;
                ge::net::C2S_RequestChunk wire{};
                ge::net::encode_request(req, wire);
                if (net_loop)
                    (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_REQUEST_CHUNK, io::UdpChan::Reliable,
                                                 io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            } else if (cmd.type == NetCmdType::BlockEdit) {
                ge::net::BlockEdit edit{};
                edit.wx = cmd.wx;
                edit.wy = cmd.wy;
                edit.wz = cmd.wz;
                edit.block_id = cmd.block_id;
                edit.state = cmd.block_state;
                ge::net::C2S_BlockEdit wire{};
                ge::net::encode_c2s_block_edit(edit, wire);
                if (net_loop)
                    (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_BLOCK_EDIT, io::UdpChan::Reliable,
                                                 io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            } else if (cmd.type == NetCmdType::Chat) {
                ge::net::ChatLine line{};
                line.kind = ge::net::CHAT_KIND_PLAYER;
                line.name_len = cmd.chat_name_len;
                if (line.name_len > ge::net::CHAT_NAME_MAX) line.name_len = static_cast<io::u8>(ge::net::CHAT_NAME_MAX);
                line.text_len = cmd.chat_text_len;
                if (line.text_len > ge::net::CHAT_TEXT_MAX) line.text_len = static_cast<io::u8>(ge::net::CHAT_TEXT_MAX);
                for (io::u32 i = 0; i < ge::net::CHAT_NAME_MAX; ++i)
                    line.name[i] = (i < line.name_len) ? cmd.chat_name[i] : '\0';
                for (io::u32 i = 0; i < ge::net::CHAT_TEXT_MAX; ++i)
                    line.text[i] = (i < line.text_len) ? cmd.chat_text[i] : '\0';
                ge::net::C2S_Chat wire{};
                ge::net::encode_c2s_chat(line, wire);
                if (net_loop)
                    (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_CHAT, io::UdpChan::Reliable,
                                                 io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            } else if (cmd.type == NetCmdType::InventoryAction) {
                ge::net::C2S_InventoryAction wire{};
                ge::net::encode_c2s_inventory_action(cmd.inventory_action, wire);
                if (net_loop)
                    (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_INVENTORY_ACTION, io::UdpChan::Reliable,
                                                 io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            } else if (cmd.type == NetCmdType::MeleeAttack) {
                ge::net::MeleeAttackSample sample{};
                sample.client_ms = static_cast<io::u32>(now_ms - net_boot_ms);
                sample.yaw = cmd.melee_yaw;
                sample.pitch = cmd.melee_pitch;
                ge::net::C2S_MeleeAttack wire{};
                ge::net::encode_c2s_melee_attack(sample, wire);
                if (net_loop)
                    (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_MELEE_ATTACK, io::UdpChan::Unreliable,
                                                 io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            } else if (cmd.type == NetCmdType::WardConfigAction) {
                ge::net::C2S_WardConfigAction wire{};
                ge::net::encode_c2s_ward_config_action(cmd.ward_config_action, wire);
                if (net_loop)
                    (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_WARD_CONFIG_ACTION, io::UdpChan::Reliable,
                                                 io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
            }
            --send_budget;
        }
    }

    inline bool InitNetworkClient() noexcept {
        if (!net_recv_buf || !net_cmd_ring || !net_incoming_ring || !net_chunk_assembly)
            return false;
        if (!net_udp.open(io::Protocol::UDP))
            return false;

        io::Endpoint local{};
        local.addr_be = 0u;
        local.port_be = io::h2ns(0u);
        if (!net_udp.bind(local)) {
            net_udp.close();
            return false;
        }
        (void)net_udp.set_blocking(false);

        if (!net_loop) net_loop = new io::EventLoop<1200, 4096>{};
        if (!net_loop || !net_loop->init(/*is_server=*/false)) {
            net_udp.close();
            return false;
        }

        net_boot_ms = io::monotonic_ms();
        net_ready.store(1u);
        net_connect_wanted.store(0u);
        net_state.store(0u);
        net_next_handshake_ms = 0u;
        net_last_ping_sent_ms = 0u;
        net_last_pos_sent_ms = 0u;
        net_next_request_id = 1u;
        net_pos_yaw = camera.yaw;
        net_pos_pitch = camera.pitch;
        net_pos_action_flags = 0u;
        net_ping_avg_ms.store(0u);
        net_last_roster_report_ms = 0u;
        net_last_roster_quality = 0xFFu;
        if (net_chunk_assembly)
            net_chunk_assembly->reset();
        ResetNetCommandQueue();
        ResetNetIncomingQueue();
        ResetNetWorldTimeSync();
        ClearPlayerRoster();
        ClearRemotePlayers();
        ClearRegionStateCache();
        if (!net_thread.start(&Window::NetThreadEntry, this)) {
            net_ready.store(0u);
            net_udp.close();
            return false;
        }
        return true;
    }

    inline void ShutdownNetworkClient() noexcept {
        if (!net_ready.load()) return;
        net_connect_wanted.store(0u);
        if (net_loop)
            net_loop->stop();
        if (net_thread.running())
            (void)net_thread.join();
        net_udp.close();
        delete net_loop;
        net_loop = nullptr;
        net_ready.store(0u);
        net_state.store(0u);
        ClearRegionStateCache();
    }


