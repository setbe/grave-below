    inline bool InitMainStorage() noexcept {
        if (!worker_pool)
            worker_pool = &worker_pool_storage;

        if (!chat_log) {
            chat_log = chat_log_storage;
            for (io::u32 i = 0; i < CHAT_LOG_CAP; ++i)
                chat_log[i] = {};
        }

        if (!player_roster_names)
            player_roster_names = player_roster_names_storage;

        if (!player_roster_ping)
            player_roster_ping = player_roster_ping_storage;

        if (!remote_players) {
            remote_players = remote_players_storage;
            for (io::u32 i = 0u; i < REMOTE_PLAYER_CAP; ++i)
                remote_players[i] = {};
        }

        if (!world_actor_ecs) {
            world_actor_ecs = &world_actor_ecs_storage;
            world_actor_ecs->Reset();
        }

        if (!player_ecs) {
            player_ecs = &player_ecs_storage;
            player_ecs->Reset();
            player_ecs->Activate(0u);
            SyncLocalPlayerEcsFromRuntime();
        }

        if (!sand_source_events) {
            sand_source_events = sand_source_events_storage;
            for (io::u32 i = 0; i < SAND_SOURCE_EVENT_CAP; ++i)
                sand_source_events[i] = {};
        }

        if (!sand_lerp_visuals) {
            sand_lerp_visuals = sand_lerp_visuals_storage;
            for (io::u32 i = 0; i < SAND_LERP_VISUAL_CAP; ++i)
                sand_lerp_visuals[i] = {};
        }

        if (!block_face_uv) {
            const io::usize count = static_cast<io::usize>(ge::voxel::BLOCK_COUNT) * static_cast<io::usize>(FACE_INDEX_COUNT);
            block_face_uv = block_face_uv_storage;
            for (io::usize i = 0; i < count; ++i)
                block_face_uv[i] = {};
        }

        if (!block_map_tint) {
            block_map_tint = block_map_tint_storage;
            for (io::usize i = 0; i < static_cast<io::usize>(ge::voxel::BLOCK_COUNT); ++i)
                block_map_tint[i] = ge::ui::Color(0.52f, 0.56f, 0.62f, 0.28f);
            block_map_tint_ready = false;
        }

        ClearRegionStateCache();

        if (!chunk_job_slots)
            chunk_job_slots = chunk_job_slots_storage;

        if (!chunk_job_tasks) {
            chunk_job_tasks = chunk_job_tasks_storage;
            for (io::u32 i = 0; i < CHUNK_JOB_MAX_SLOTS; ++i)
                chunk_job_tasks[i] = {};
        }

        ClearPlayerRoster();

        return true;
    }

    inline void ShutdownMainStorage() noexcept {
        chat_log = nullptr;

        player_roster_names = nullptr;
        player_roster_ping = nullptr;
        remote_players = nullptr;

        world_actor_ecs = nullptr;

        player_ecs = nullptr;

        sand_source_events = nullptr;

        sand_lerp_visuals = nullptr;

        chunk_job_slots = nullptr;

        chunk_job_tasks = nullptr;

        block_face_uv = nullptr;
        block_map_tint = nullptr;
        block_map_tint_ready = false;

        if (worker_pool) {
            worker_pool->shutdown(true);
            worker_pool = nullptr;
        }
    }

    inline bool InitNetStorage() noexcept {
        if (!net_recv_buf) {
            net_recv_buf = net_recv_buf_storage;
            for (io::u32 i = 0; i < NET_RECV_BUF_CAP; ++i)
                net_recv_buf[i] = 0u;
        }

        if (!net_cmd_ring) {
            net_cmd_ring = net_cmd_ring_storage;
            for (io::u32 i = 0; i < NET_CMD_CAP; ++i)
                net_cmd_ring[i] = {};
        }

        if (!net_incoming_ring) {
            net_incoming_ring = reinterpret_cast<NetIncomingChunk*>(net_incoming_ring_storage);
            for (io::u32 i = 0; i < NET_INCOMING_CAP; ++i)
                new (&net_incoming_ring[i]) NetIncomingChunk{};
        }

        if (!net_chunk_assembly) {
            net_chunk_assembly = new (net_chunk_assembly_storage) ge::net::ChunkAssembly{};
        }

        return true;
    }

    inline void ShutdownNetStorage() noexcept {
        if (net_chunk_assembly) {
            net_chunk_assembly->~ChunkAssembly();
            net_chunk_assembly = nullptr;
        }
        if (net_incoming_ring) {
            for (io::u32 i = 0; i < NET_INCOMING_CAP; ++i)
                net_incoming_ring[i].~NetIncomingChunk();
            net_incoming_ring = nullptr;
        }
        net_cmd_ring = nullptr;
        net_recv_buf = nullptr;
        net_cmd_head = 0;
        net_cmd_tail = 0;
        net_cmd_count = 0;
        net_incoming_head = 0;
        net_incoming_tail = 0;
        net_incoming_count = 0;
    }

    inline void ResetNetCommandQueue() noexcept {
        net_cmd_lock.lock();
        net_cmd_head = 0;
        net_cmd_tail = 0;
        net_cmd_count = 0;
        net_cmd_lock.unlock();
    }

    inline void ResetNetIncomingQueue() noexcept {
        net_incoming_lock.lock();
        net_incoming_head = 0;
        net_incoming_tail = 0;
        net_incoming_count = 0;
        net_incoming_lock.unlock();
    }

    IO_NODISCARD inline bool EnqueueNetChunkRequest(const ge::voxel::ChunkCoord& coord, io::u8 lod = 0u) noexcept {
        if (!net_cmd_ring) return false;
        bool ok = false;
        static constexpr io::u32 NET_CMD_GAMEPLAY_RESERVE = 128u;
        net_cmd_lock.lock();
        if (net_cmd_count < (NET_CMD_CAP - NET_CMD_GAMEPLAY_RESERVE)) {
            NetChunkCmd& cmd = net_cmd_ring[net_cmd_tail];
            cmd.type = NetCmdType::RequestChunk;
            cmd.coord = coord;
            cmd.lod = lod;
            cmd.wx = 0;
            cmd.wy = 0;
            cmd.wz = 0;
            cmd.block_id = 0u;
            cmd.block_state = 0u;
            cmd.melee_yaw = 0.f;
            cmd.melee_pitch = 0.f;
            cmd.inventory_action = {};
            cmd.ward_config_action = {};
            cmd.chat_name_len = 0u;
            cmd.chat_text_len = 0u;
            cmd.chat_name[0] = '\0';
            cmd.chat_text[0] = '\0';
            net_cmd_tail = (net_cmd_tail + 1u) % NET_CMD_CAP;
            ++net_cmd_count;
            ok = true;
        }
        net_cmd_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueNetBlockEdit(io::i32 wx, io::i32 wy, io::i32 wz,
                                                 ge::voxel::BlockId block_id, io::u16 block_state) noexcept {
        if (!net_cmd_ring) return false;
        bool ok = false;
        net_cmd_lock.lock();
        if (net_cmd_count >= NET_CMD_CAP && net_cmd_count > 0u) {
            io::u32 kept = 0u;
            for (io::u32 i = 0u; i < net_cmd_count; ++i) {
                const io::u32 src = (net_cmd_head + i) % NET_CMD_CAP;
                const NetChunkCmd cmd = net_cmd_ring[src];
                if (cmd.type == NetCmdType::RequestChunk)
                    continue;
                const io::u32 dst = (net_cmd_head + kept) % NET_CMD_CAP;
                if (dst != src)
                    net_cmd_ring[dst] = cmd;
                ++kept;
            }
            net_cmd_count = kept;
            net_cmd_tail = (net_cmd_head + kept) % NET_CMD_CAP;
        }
        if (net_cmd_count < NET_CMD_CAP) {
            NetChunkCmd& cmd = net_cmd_ring[net_cmd_tail];
            cmd.type = NetCmdType::BlockEdit;
            cmd.coord = {};
            cmd.lod = 0u;
            cmd.wx = wx;
            cmd.wy = wy;
            cmd.wz = wz;
            cmd.block_id = ge::voxel::block_index(block_id);
            cmd.block_state = block_state;
            cmd.melee_yaw = 0.f;
            cmd.melee_pitch = 0.f;
            cmd.inventory_action = {};
            cmd.ward_config_action = {};
            cmd.chat_name_len = 0u;
            cmd.chat_text_len = 0u;
            cmd.chat_name[0] = '\0';
            cmd.chat_text[0] = '\0';
            net_cmd_tail = (net_cmd_tail + 1u) % NET_CMD_CAP;
            ++net_cmd_count;
            ok = true;
        }
        net_cmd_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueNetChatMessage(io::char_view name, io::char_view text) noexcept {
        if (!net_cmd_ring) return false;
        const io::char_view trimmed_text = TrimChatText(text);
        if (trimmed_text.empty()) return false;

        bool ok = false;
        net_cmd_lock.lock();
        if (net_cmd_count < NET_CMD_CAP) {
            NetChunkCmd& cmd = net_cmd_ring[net_cmd_tail];
            cmd.type = NetCmdType::Chat;
            cmd.coord = {};
            cmd.lod = 0u;
            cmd.wx = 0;
            cmd.wy = 0;
            cmd.wz = 0;
            cmd.block_id = 0u;
            cmd.block_state = 0u;
            cmd.melee_yaw = 0.f;
            cmd.melee_pitch = 0.f;
            cmd.inventory_action = {};
            cmd.ward_config_action = {};

            io::usize name_n = name.size();
            if (name_n > ge::net::CHAT_NAME_MAX) name_n = ge::net::CHAT_NAME_MAX;
            cmd.chat_name_len = static_cast<io::u8>(name_n);
            for (io::u32 i = 0; i < ge::net::CHAT_NAME_MAX; ++i)
                cmd.chat_name[i] = (i < cmd.chat_name_len) ? name[i] : '\0';
            cmd.chat_name[cmd.chat_name_len] = '\0';

            io::usize text_n = trimmed_text.size();
            if (text_n > ge::net::CHAT_TEXT_MAX) text_n = ge::net::CHAT_TEXT_MAX;
            cmd.chat_text_len = static_cast<io::u8>(text_n);
            for (io::u32 i = 0; i < ge::net::CHAT_TEXT_MAX; ++i)
                cmd.chat_text[i] = (i < cmd.chat_text_len) ? trimmed_text[i] : '\0';
            cmd.chat_text[cmd.chat_text_len] = '\0';

            net_cmd_tail = (net_cmd_tail + 1u) % NET_CMD_CAP;
            ++net_cmd_count;
            ok = true;
        }
        net_cmd_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool DequeueNetChunkRequest(NetChunkCmd& out_cmd) noexcept {
        if (!net_cmd_ring) return false;
        bool ok = false;
        net_cmd_lock.lock();
        if (net_cmd_count > 0u) {
            out_cmd = net_cmd_ring[net_cmd_head];
            net_cmd_head = (net_cmd_head + 1u) % NET_CMD_CAP;
            --net_cmd_count;
            ok = true;
        }
        net_cmd_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueNetInventoryAction(const ge::net::InventoryAction& action) noexcept {
        if (!net_cmd_ring) return false;
        bool ok = false;
        net_cmd_lock.lock();
        if (net_cmd_count < NET_CMD_CAP) {
            NetChunkCmd& cmd = net_cmd_ring[net_cmd_tail];
            cmd.type = NetCmdType::InventoryAction;
            cmd.coord = {};
            cmd.lod = 0u;
            cmd.wx = 0;
            cmd.wy = 0;
            cmd.wz = 0;
            cmd.block_id = 0u;
            cmd.block_state = 0u;
            cmd.melee_yaw = 0.f;
            cmd.melee_pitch = 0.f;
            cmd.inventory_action = action;
            cmd.ward_config_action = {};
            cmd.chat_name_len = 0u;
            cmd.chat_text_len = 0u;
            cmd.chat_name[0] = '\0';
            cmd.chat_text[0] = '\0';
            net_cmd_tail = (net_cmd_tail + 1u) % NET_CMD_CAP;
            ++net_cmd_count;
            ok = true;
        }
        net_cmd_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueNetMeleeAttack(float yaw, float pitch) noexcept {
        if (!net_cmd_ring) return false;
        bool ok = false;
        net_cmd_lock.lock();
        if (net_cmd_count < NET_CMD_CAP) {
            NetChunkCmd& cmd = net_cmd_ring[net_cmd_tail];
            cmd.type = NetCmdType::MeleeAttack;
            cmd.coord = {};
            cmd.lod = 0u;
            cmd.wx = 0;
            cmd.wy = 0;
            cmd.wz = 0;
            cmd.block_id = 0u;
            cmd.block_state = 0u;
            cmd.melee_yaw = yaw;
            cmd.melee_pitch = pitch;
            cmd.inventory_action = {};
            cmd.ward_config_action = {};
            cmd.chat_name_len = 0u;
            cmd.chat_text_len = 0u;
            cmd.chat_name[0] = '\0';
            cmd.chat_text[0] = '\0';
            net_cmd_tail = (net_cmd_tail + 1u) % NET_CMD_CAP;
            ++net_cmd_count;
            ok = true;
        }
        net_cmd_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueNetWardConfigAction(const ge::net::WardConfigActionSample& action) noexcept {
        if (!net_cmd_ring) return false;
        bool ok = false;
        net_cmd_lock.lock();
        if (net_cmd_count < NET_CMD_CAP) {
            NetChunkCmd& cmd = net_cmd_ring[net_cmd_tail];
            cmd.type = NetCmdType::WardConfigAction;
            cmd.coord = {};
            cmd.lod = 0u;
            cmd.wx = 0;
            cmd.wy = 0;
            cmd.wz = 0;
            cmd.block_id = 0u;
            cmd.block_state = 0u;
            cmd.melee_yaw = 0.f;
            cmd.melee_pitch = 0.f;
            cmd.inventory_action = {};
            cmd.ward_config_action = action;
            cmd.chat_name_len = 0u;
            cmd.chat_text_len = 0u;
            cmd.chat_name[0] = '\0';
            cmd.chat_text[0] = '\0';
            net_cmd_tail = (net_cmd_tail + 1u) % NET_CMD_CAP;
            ++net_cmd_count;
            ok = true;
        }
        net_cmd_lock.unlock();
        return ok;
    }

    inline void PurgeNetChunkRequestCommands() noexcept {
        if (!net_cmd_ring) return;
        net_cmd_lock.lock();
        if (net_cmd_count == 0u) {
            net_cmd_lock.unlock();
            return;
        }

        io::u32 kept = 0u;
        for (io::u32 i = 0u; i < net_cmd_count; ++i) {
            const io::u32 src = (net_cmd_head + i) % NET_CMD_CAP;
            const NetChunkCmd cmd = net_cmd_ring[src];
            if (cmd.type == NetCmdType::RequestChunk)
                continue;
            const io::u32 dst = (net_cmd_head + kept) % NET_CMD_CAP;
            if (dst != src)
                net_cmd_ring[dst] = cmd;
            ++kept;
        }
        net_cmd_count = kept;
        net_cmd_tail = (net_cmd_head + kept) % NET_CMD_CAP;
        net_cmd_lock.unlock();
    }

    IO_NODISCARD inline bool EnqueueIncomingChunk(const ge::voxel::ChunkData& chunk) noexcept {
        if (!net_incoming_ring) return false;
        bool ok = false;
        net_incoming_lock.lock();
        if (net_incoming_count < NET_INCOMING_CAP) {
            NetIncomingChunk& item = net_incoming_ring[net_incoming_tail];
            item.type = NetIncomingType::Chunk;
            CopyChunkData(item.chunk, chunk);
            item.edit = {};
            net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
            ++net_incoming_count;
            ok = true;
        }
        net_incoming_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueIncomingBlockEdit(const ge::net::BlockEdit& edit) noexcept {
        if (!net_incoming_ring) return false;
        bool ok = false;
        net_incoming_lock.lock();
        if (net_incoming_count >= NET_INCOMING_CAP && net_incoming_count > 0u) {
            const io::u32 head = net_incoming_head;
            const NetIncomingType head_type = net_incoming_ring[head].type;
            if (head_type == NetIncomingType::Chunk || head_type == NetIncomingType::Chat ||
                head_type == NetIncomingType::WorldActor || head_type == NetIncomingType::RegionState) {
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
            }
        }
        if (net_incoming_count < NET_INCOMING_CAP) {
            NetIncomingChunk& item = net_incoming_ring[net_incoming_tail];
            item.type = NetIncomingType::BlockEdit;
            item.edit = edit;
            net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
            ++net_incoming_count;
            ok = true;
        }
        net_incoming_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueIncomingChat(const ge::net::ChatLine& line) noexcept {
        ObserveChatRosterEntry(line);
        PushChatLine(line);
        return true;
    }

    IO_NODISCARD inline bool DequeueIncomingChunk(ge::voxel::ChunkData& out_chunk) noexcept {
        if (!net_incoming_ring) return false;
        bool ok = false;
        net_incoming_lock.lock();
        if (net_incoming_count > 0u) {
            CopyChunkData(out_chunk, net_incoming_ring[net_incoming_head].chunk);
            net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
            --net_incoming_count;
            ok = true;
        }
        net_incoming_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueIncomingWorldActor(const ge::net::WorldActorSample& actor) noexcept {
        if (!net_incoming_ring) return false;
        bool ok = false;
        net_incoming_lock.lock();
        if (net_incoming_count >= NET_INCOMING_CAP && net_incoming_count > 0u) {
            const io::u32 head = net_incoming_head;
            const NetIncomingType head_type = net_incoming_ring[head].type;
            if (head_type == NetIncomingType::Chunk || head_type == NetIncomingType::Chat) {
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
            }
        }
        if (net_incoming_count < NET_INCOMING_CAP) {
            NetIncomingChunk& item = net_incoming_ring[net_incoming_tail];
            item.type = NetIncomingType::WorldActor;
            item.actor = actor;
            net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
            ++net_incoming_count;
            ok = true;
        }
        net_incoming_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueIncomingInventoryState(const ge::net::InventoryStateSample& state) noexcept {
        if (!net_incoming_ring) return false;
        bool ok = false;
        net_incoming_lock.lock();
        if (net_incoming_count > 0u) {
            io::u32 idx = net_incoming_head;
            for (io::u32 n = 0u; n < net_incoming_count; ++n) {
                if (net_incoming_ring[idx].type == NetIncomingType::InventoryState) {
                    net_incoming_ring[idx].inventory = state;
                    ok = true;
                    break;
                }
                idx = (idx + 1u) % NET_INCOMING_CAP;
            }
        }
        if (ok) {
            net_incoming_lock.unlock();
            return true;
        }
        if (net_incoming_count >= NET_INCOMING_CAP && net_incoming_count > 0u) {
            const io::u32 head = net_incoming_head;
            const NetIncomingType head_type = net_incoming_ring[head].type;
            if (head_type == NetIncomingType::Chunk || head_type == NetIncomingType::Chat || head_type == NetIncomingType::BlockEdit) {
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
            }
        }
        if (net_incoming_count >= NET_INCOMING_CAP && net_incoming_count > 0u) {
            net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
            --net_incoming_count;
        }
        if (net_incoming_count < NET_INCOMING_CAP) {
            NetIncomingChunk& item = net_incoming_ring[net_incoming_tail];
            item.type = NetIncomingType::InventoryState;
            item.inventory = state;
            net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
            ++net_incoming_count;
            ok = true;
        }
        net_incoming_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueIncomingWardConfigState(const ge::net::WardConfigStateSample& state) noexcept {
        if (!net_incoming_ring) return false;
        bool ok = false;
        net_incoming_lock.lock();
        if (net_incoming_count >= NET_INCOMING_CAP && net_incoming_count > 0u) {
            net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
            --net_incoming_count;
        }
        if (net_incoming_count < NET_INCOMING_CAP) {
            NetIncomingChunk& item = net_incoming_ring[net_incoming_tail];
            item.type = NetIncomingType::WardConfigState;
            item.ward_config = state;
            net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
            ++net_incoming_count;
            ok = true;
        }
        net_incoming_lock.unlock();
        return ok;
    }

    IO_NODISCARD inline bool EnqueueIncomingRegionState(const ge::net::RegionStateSample& state) noexcept {
        if (!net_incoming_ring) return false;
        bool ok = false;
        net_incoming_lock.lock();
        if (net_incoming_count > 0u) {
            io::u32 idx = net_incoming_head;
            for (io::u32 n = 0u; n < net_incoming_count; ++n) {
                if (net_incoming_ring[idx].type == NetIncomingType::RegionState) {
                    net_incoming_ring[idx].region_state = state;
                    ok = true;
                    break;
                }
                idx = (idx + 1u) % NET_INCOMING_CAP;
            }
        }
        if (!ok) {
            if (net_incoming_count >= NET_INCOMING_CAP && net_incoming_count > 0u) {
                net_incoming_head = (net_incoming_head + 1u) % NET_INCOMING_CAP;
                --net_incoming_count;
            }
            if (net_incoming_count < NET_INCOMING_CAP) {
                NetIncomingChunk& item = net_incoming_ring[net_incoming_tail];
                item.type = NetIncomingType::RegionState;
                item.region_state = state;
                net_incoming_tail = (net_incoming_tail + 1u) % NET_INCOMING_CAP;
                ++net_incoming_count;
                ok = true;
            }
        }
        net_incoming_lock.unlock();
        return ok;
    }

    inline void ResetNetTpsHistory() noexcept {
        net_tps_hist_head = 0u;
        net_tps_hist_count = 0u;
        net_tps_hist_sum = 0u;
        for (io::u32 i = 0; i < NET_TPS_HIST_CAP; ++i)
            net_tps_hist[i] = 0u;
        net_tps_avg_x100.store(0u);
    }

    inline void ResetNetWorldTimeSync() noexcept {
        net_world_time_lock.lock();
        net_world_time_synced = false;
        net_world_phase_ms = 0u;
        net_world_day_ms = 1200000u;   // 20 min default
        net_world_night_ms = 900000u;  // 15 min default
        net_world_sync_local_ms = io::monotonic_ms();
        net_world_time_lock.unlock();
    }

    inline void PushNetWorldTimeSample(const ge::net::WorldTimeSample& sample) noexcept {
        io::u32 day_ms = sample.day_ms;
        io::u32 night_ms = sample.night_ms;
        if (day_ms < 1000u) day_ms = 1000u;
        if (night_ms < 1000u) night_ms = 1000u;
        io::u32 cycle_ms = day_ms + night_ms;
        if (cycle_ms == 0u) cycle_ms = 1u;
        io::u32 phase_ms = sample.cycle_pos_ms;
        if (phase_ms >= cycle_ms) phase_ms %= cycle_ms;

        net_world_time_lock.lock();
        net_world_time_synced = true;
        net_world_phase_ms = phase_ms;
        net_world_day_ms = day_ms;
        net_world_night_ms = night_ms;
        net_world_sync_local_ms = io::monotonic_ms();
        net_world_time_lock.unlock();
    }

    inline void PushNetTpsSample(io::u16 tps_x100) noexcept {
        if (net_tps_hist_count < NET_TPS_HIST_CAP) {
            const io::u32 idx = (net_tps_hist_head + net_tps_hist_count) % NET_TPS_HIST_CAP;
            net_tps_hist[idx] = tps_x100;
            net_tps_hist_sum += tps_x100;
            ++net_tps_hist_count;
        } else {
            net_tps_hist_sum -= net_tps_hist[net_tps_hist_head];
            net_tps_hist[net_tps_hist_head] = tps_x100;
            net_tps_hist_sum += tps_x100;
            net_tps_hist_head = (net_tps_hist_head + 1u) % NET_TPS_HIST_CAP;
        }

        io::u32 avg_x100 = 0u;
        if (net_tps_hist_count > 0u)
            avg_x100 = net_tps_hist_sum / net_tps_hist_count;
        if (avg_x100 > 100000u) avg_x100 = 100000u;
        net_tps_avg_x100.store(avg_x100);
    }

    static inline void CopyChunkData(ge::voxel::ChunkData& dst, const ge::voxel::ChunkData& src) noexcept {
        dst.coord = src.coord;
        dst.non_air_count = src.non_air_count;
        dst.version = src.version;
        dst.dirty_mesh = src.dirty_mesh;
        dst.dirty_neighbors = src.dirty_neighbors;
        dst.generated = src.generated;
        for (io::u32 i = 0; i < ge::voxel::CHUNK_VOLUME; ++i)
            dst.blocks[i] = src.blocks[i];
    }

