        // Fluid simulation (water/blood/slime), block edits, chat, and initialization.
        inline void note_packet_backpressure() noexcept {
            ++stats.send_backpressure;
            ++stats.send_backpressure_packets;
        }

        inline void clear_slot_backpressure_latch(WorkerSlot& slot) noexcept {
            slot.send_backpressure_latched = false;
        }

        inline void note_chunk_backpressure(WorkerSlot& slot) noexcept {
            ++stats.send_backpressure_ticks;
            if (slot.send_backpressure_latched) return;
            slot.send_backpressure_latched = true;
            ++stats.send_backpressure;
        }

        inline void simulate_water(io::u64 now_ms) noexcept {
            if (now_ms < stats.water_next_step_ms) return;
            stats.water_next_step_ms = now_ms + WATER_STEP_INTERVAL_MS;
            if (!water_touched || !water_active) return;
            if (active_peers() == 0u) return;

            struct FluidProfile {
                io::u16 down_max = 1u;
                io::u16 diag_max = 1u;
                io::u16 side_max = 1u;
                io::u8 step_mod = 1u;
            };

            const auto profile_for = [](ge::voxel::FluidKind kind) noexcept -> FluidProfile {
                static constexpr FluidProfile PROFILE_BY_KIND[4]{
                    FluidProfile{ 0u, 0u, 0u, 1u }, // None
                    FluidProfile{ 2u, 1u, 1u, 1u }, // Water
                    FluidProfile{ 1u, 1u, 1u, 2u }, // Blood
                    FluidProfile{ 1u, 1u, 1u, 3u }  // Slime
                };
                const io::u8 idx = static_cast<io::u8>(kind);
                if (idx >= 4u) return FluidProfile{};
                return PROFILE_BY_KIND[idx];
            };

            const auto top_kind = [](const ge::voxel::FluidStack& s) noexcept -> ge::voxel::FluidKind {
                if (s.top_kind != ge::voxel::FluidKind::None && s.top_level > 0u)
                    return s.top_kind;
                if (s.bottom_kind != ge::voxel::FluidKind::None && s.bottom_level > 0u)
                    return s.bottom_kind;
                return ge::voxel::FluidKind::None;
            };

            const auto bottom_kind = [](const ge::voxel::FluidStack& s) noexcept -> ge::voxel::FluidKind {
                if (s.bottom_kind != ge::voxel::FluidKind::None && s.bottom_level > 0u)
                    return s.bottom_kind;
                if (s.top_kind != ge::voxel::FluidKind::None && s.top_level > 0u)
                    return s.top_kind;
                return ge::voxel::FluidKind::None;
            };

            const auto load_stack = [&](io::i32 wx, io::i32 wy, io::i32 wz, ge::voxel::FluidStack& out) noexcept -> bool {
                ge::voxel::BlockId id = ge::voxel::BlockId::Air;
                io::u16 state = 0u;
                if (!try_get_world_state_for_water(wx, wy, wz, id, state))
                    return false;
                out = ge::voxel::fluid_stack_from_block(id, state);
                ge::voxel::normalize_fluid_stack(out);
                return true;
            };

            const auto kind_level = [](const ge::voxel::FluidStack& s, ge::voxel::FluidKind kind) noexcept -> io::u8 {
                if (kind == ge::voxel::FluidKind::None) return 0u;
                io::u8 out = 0u;
                if (s.bottom_kind == kind) out = static_cast<io::u8>(out + s.bottom_level);
                if (s.top_kind == kind) out = static_cast<io::u8>(out + s.top_level);
                return out;
            };

            const auto remove_kind = [](ge::voxel::FluidStack& s, ge::voxel::FluidKind kind, io::u16 amount) noexcept -> bool {
                if (amount == 0u || kind == ge::voxel::FluidKind::None) return false;
                if (s.top_kind == kind && s.top_level >= amount) {
                    s.top_level = static_cast<io::u8>(s.top_level - amount);
                    return true;
                }
                if (s.bottom_kind == kind && s.bottom_level >= amount) {
                    s.bottom_level = static_cast<io::u8>(s.bottom_level - amount);
                    return true;
                }
                return false;
            };
            const auto add_kind = [](ge::voxel::FluidStack& s, ge::voxel::FluidKind kind, io::u16 amount) noexcept -> bool {
                if (amount == 0u || kind == ge::voxel::FluidKind::None) return false;
                if (s.bottom_kind == ge::voxel::FluidKind::None) {
                    s.bottom_kind = kind;
                    s.bottom_level = static_cast<io::u8>(amount);
                    return true;
                }
                if (s.bottom_kind == kind) {
                    s.bottom_level = static_cast<io::u8>(s.bottom_level + amount);
                    return true;
                }
                if (s.top_kind == ge::voxel::FluidKind::None) {
                    s.top_kind = kind;
                    s.top_level = static_cast<io::u8>(amount);
                    return true;
                }
                if (s.top_kind == kind) {
                    s.top_level = static_cast<io::u8>(s.top_level + amount);
                    return true;
                }
                return false;
            };

            if (stats.water_active_count < (WATER_SCAN_BUDGET / 4u) && !world.chunks.empty()) {
                const io::u32 chunk_count = static_cast<io::u32>(world.chunks.size());
                io::u32 scan = 0u;
                io::u32 chunk_idx = static_cast<io::u32>(stats.water_seed_chunk_cursor % chunk_count);
                io::u32 linear = stats.water_seed_linear_cursor;
                if (linear >= ge::voxel::CHUNK_VOLUME) linear = 0u;
                io::u32 no_hot_cycle = 0u;
                while (scan < HOT_SIM_SEED_SCAN_BUDGET) {
                    ge::voxel::ChunkData& chunk = world.chunks[chunk_idx];
                    if (!is_chunk_hot_any(chunk.coord)) {
                        ++scan;
                        ++no_hot_cycle;
                        linear = 0u;
                        chunk_idx = (chunk_idx + 1u) % chunk_count;
                        if (no_hot_cycle >= chunk_count) break;
                        continue;
                    }
                    no_hot_cycle = 0u;
                    if (linear >= ge::voxel::CHUNK_VOLUME) linear = 0u;
                    const ge::voxel::BlockState st = chunk.blocks[linear];
                    ge::voxel::BlockId sid = ge::voxel::BlockId::Air;
                    if (st.id < ge::voxel::BLOCK_COUNT)
                        sid = static_cast<ge::voxel::BlockId>(st.id);
                    if (ge::voxel::is_fluid_block_id(sid)) {
                        const io::u32 lx = linear & 31u;
                        const io::u32 lz = (linear >> 5u) & 31u;
                        const io::u32 ly = (linear >> 10u) & 31u;
                        const io::i32 wx = chunk.coord.x * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lx);
                        const io::i32 wy = chunk.coord.y * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(ly);
                        const io::i32 wz = chunk.coord.z * static_cast<io::i32>(ge::voxel::CHUNK_SIZE) + static_cast<io::i32>(lz);
                        enqueue_water_cell(wx, wy, wz, now_ms);
                    }
                    ++linear;
                    ++scan;
                    if (linear >= ge::voxel::CHUNK_VOLUME) {
                        linear = 0u;
                        chunk_idx = (chunk_idx + 1u) % chunk_count;
                    }
                }
                stats.water_seed_chunk_cursor = chunk_idx;
                stats.water_seed_linear_cursor = linear;
            }

            if (stats.water_active_count == 0u)
                return;

            io::u32 touched_count = 0u;
            io::u32 processed = 0u;
            io::u32 moved = 0u;
            const bool send_to_peers = active_peers() > 0u;
            io::u32 move_budget = (WATER_EDIT_BUDGET > 0u) ? WATER_EDIT_BUDGET : 1u;
            if (stats.water_active_count > 8192u) move_budget *= 2u;
            if (stats.water_active_count > 32768u) move_budget *= 2u;
            if (move_budget > 384u) move_budget = 384u;
            io::u32 process_budget = WATER_SCAN_BUDGET + move_budget * 8u;
            if (process_budget > (WATER_SCAN_BUDGET * 20u))
                process_budget = WATER_SCAN_BUDGET * 20u;

            const auto commit_single_stack = [&](io::i32 wx, io::i32 wy, io::i32 wz,
                                                 const ge::voxel::FluidStack& stack) noexcept -> bool {
                ge::voxel::BlockId old_id = ge::voxel::BlockId::Air;
                io::u16 old_state = 0u;
                if (!try_get_world_state_for_water(wx, wy, wz, old_id, old_state))
                    return false;

                ge::voxel::BlockId new_id = ge::voxel::BlockId::Air;
                io::u16 new_state = 0u;
                ge::voxel::FluidStack st = stack;
                ge::voxel::normalize_fluid_stack(st);
                ge::voxel::fluid_stack_to_block(st, new_id, new_state);
                if (new_id == old_id && new_state == old_state)
                    return false;

                ge::voxel::ChunkCoord cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
                if (!ensure_world_chunk(cc))
                    return false;
                if (!world.set_world_state(wx, wy, wz, new_id, new_state, false))
                    return false;

                hot_update_flags_cell(wx, wy, wz, new_id, now_ms);
                mark_touched_chunk(water_touched, touched_count, WATER_SAVE_CAP, cc);
                enqueue_water_neighborhood(wx, wy, wz, now_ms);
                if (send_to_peers) {
                    const bool is_fluid = new_id != ge::voxel::BlockId::Air;
                    send_world_cell_edit(wx, wy, wz, new_id, new_state, now_ms, is_fluid);
                }
                return true;
            };

            const auto commit_transfer = [&](io::i32 sx, io::i32 sy, io::i32 sz,
                                             io::i32 tx, io::i32 ty, io::i32 tz,
                                             ge::voxel::FluidKind moving_kind,
                                             io::u16 max_layers,
                                             bool diagonal_path) noexcept -> bool {
                if (max_layers == 0u || moving_kind == ge::voxel::FluidKind::None)
                    return false;

                ge::voxel::FluidStack src{};
                if (!load_stack(sx, sy, sz, src))
                    return false;
                const io::u8 src_total = ge::voxel::fluid_total_level(src);
                if (src_total == 0u)
                    return false;

                io::u8 src_available = 0u;
                if (src.top_kind == moving_kind && src.top_level > 0u)
                    src_available = src.top_level;
                else if (src.bottom_kind == moving_kind && src.bottom_level > 0u)
                    src_available = src.bottom_level;
                if (src_available == 0u)
                    return false;

                ge::voxel::BlockId dst_id = ge::voxel::BlockId::Air;
                io::u16 dst_state = 0u;
                if (!try_get_world_state_for_water(tx, ty, tz, dst_id, dst_state))
                    return false;
                if (!water_is_fillable(dst_id))
                    return false;
                if (hot_cell_is_collision(tx, ty, tz, now_ms))
                    return false;

                ge::voxel::FluidStack dst = ge::voxel::fluid_stack_from_block(dst_id, dst_state);
                ge::voxel::normalize_fluid_stack(dst);
                const io::u8 dst_total = ge::voxel::fluid_total_level(dst);

                if (diagonal_path) {
                    const io::i32 dx = tx - sx;
                    const io::i32 dz = tz - sz;
                    if (dx != 0 && dz != 0) {
                        const bool a = hot_cell_is_collision(sx + dx, sy, sz, now_ms);
                        const bool b = hot_cell_is_collision(sx, sy, sz + dz, now_ms);
                        if (a && b) return false;
                    } else if (dx != 0 || dz != 0) {
                        if (hot_cell_is_collision(sx + dx, sy, sz + dz, now_ms))
                            return false;
                    }
                }

                ge::voxel::ChunkCoord src_cc{};
                ge::voxel::ChunkCoord dst_cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(sx, sy, sz, src_cc, lx, ly, lz);
                ge::voxel::split_world_coord(tx, ty, tz, dst_cc, lx, ly, lz);
                if (!ensure_world_chunk(src_cc)) return false;
                if (!ensure_world_chunk(dst_cc)) return false;

                const io::u8 moving_density = ge::voxel::fluid_density(moving_kind);
                const ge::voxel::FluidKind dst_heaviest = bottom_kind(dst);
                const ge::voxel::FluidKind dst_lightest = top_kind(dst);
                const io::u8 dens_heavy = ge::voxel::fluid_density(dst_heaviest);
                const io::u8 dens_light = ge::voxel::fluid_density(dst_lightest);

                bool exchange_mode = false;
                ge::voxel::FluidKind displaced_kind = ge::voxel::FluidKind::None;
                io::i32 exchange_dy = 0;

                if (dst_total >= ge::voxel::FLUID_LEVEL_MAX) {
                    const io::i32 dy_move = ty - sy;
                    exchange_dy = dy_move;
                    if (dy_move < 0) {
                        // Moving down: only denser fluid may displace a lighter component.
                        if (moving_density <= dens_light)
                            return false;
                        displaced_kind = dst_lightest;
                    } else if (dy_move > 0) {
                        // Moving up: only lighter fluid may displace a heavier component.
                        if (moving_density >= dens_heavy)
                            return false;
                        displaced_kind = dst_heaviest;
                    } else {
                        // Lateral exchange on full cells is the main source of ping-pong loops
                        // (e.g. water<->slime). Keep lateral flow only for non-full targets.
                        return false;
                    }
                    if (displaced_kind == ge::voxel::FluidKind::None || displaced_kind == moving_kind)
                        return false;
                    exchange_mode = true;
                }

                io::u16 transfer = src_available;
                if (transfer > max_layers) transfer = max_layers;
                if (!exchange_mode) {
                    io::u16 cap = static_cast<io::u16>(ge::voxel::FLUID_LEVEL_MAX - dst_total);
                    if (transfer > cap) transfer = cap;
                } else {
                    // Pick displaced component that both matches density-direction rules
                    // and can be accepted by source after removing moving layers.
                    ge::voxel::FluidKind candidates[2]{ ge::voxel::FluidKind::None, ge::voxel::FluidKind::None };
                    if (exchange_dy < 0) {
                        candidates[0] = dst_lightest;
                        candidates[1] = dst_heaviest;
                    } else {
                        candidates[0] = dst_heaviest;
                        candidates[1] = dst_lightest;
                    }
                    bool found = false;
                    for (io::u32 ci = 0u; ci < 2u; ++ci) {
                        const ge::voxel::FluidKind cand = candidates[ci];
                        if (cand == ge::voxel::FluidKind::None || cand == moving_kind)
                            continue;
                        const io::u16 displaced_available = kind_level(dst, cand);
                        if (displaced_available == 0u)
                            continue;
                        io::u16 cand_transfer = src_available;
                        if (cand_transfer > max_layers) cand_transfer = max_layers;
                        if (cand_transfer > displaced_available) cand_transfer = displaced_available;
                        if (cand_transfer == 0u)
                            continue;

                        ge::voxel::FluidStack src_probe = src;
                        if (!remove_kind(src_probe, moving_kind, cand_transfer))
                            continue;
                        if (!add_kind(src_probe, cand, cand_transfer))
                            continue;

                        displaced_kind = cand;
                        transfer = cand_transfer;
                        found = true;
                        break;
                    }
                    if (!found)
                        return false;
                }
                if (transfer == 0u)
                    return false;

                ge::voxel::FluidStack src_next = src;
                ge::voxel::FluidStack dst_next = dst;
                if (!remove_kind(src_next, moving_kind, transfer))
                    return false;
                if (exchange_mode && !add_kind(src_next, displaced_kind, transfer))
                    return false;
                if (exchange_mode && !remove_kind(dst_next, displaced_kind, transfer))
                    return false;
                if (!add_kind(dst_next, moving_kind, transfer))
                    return false;

                src = src_next;
                dst = dst_next;

                ge::voxel::normalize_fluid_stack(src);
                ge::voxel::normalize_fluid_stack(dst);

                ge::voxel::BlockId src_new_id = ge::voxel::BlockId::Air;
                ge::voxel::BlockId dst_new_id = ge::voxel::BlockId::Air;
                io::u16 src_new_state = 0u;
                io::u16 dst_new_state = 0u;
                ge::voxel::fluid_stack_to_block(src, src_new_id, src_new_state);
                ge::voxel::fluid_stack_to_block(dst, dst_new_id, dst_new_state);

                if (!world.set_world_state(tx, ty, tz, dst_new_id, dst_new_state, false))
                    return false;
                if (!world.set_world_state(sx, sy, sz, src_new_id, src_new_state, false)) {
                    (void)world.set_world_state(tx, ty, tz, dst_id, dst_state, false);
                    return false;
                }

                hot_update_flags_cell(tx, ty, tz, dst_new_id, now_ms);
                hot_update_flags_cell(sx, sy, sz, src_new_id, now_ms);
                mark_touched_chunk(water_touched, touched_count, WATER_SAVE_CAP, src_cc);
                mark_touched_chunk(water_touched, touched_count, WATER_SAVE_CAP, dst_cc);
                enqueue_water_neighborhood(sx, sy, sz, now_ms);
                enqueue_water_neighborhood(tx, ty, tz, now_ms);

                if (send_to_peers) {
                    const bool src_fluid = src_new_id != ge::voxel::BlockId::Air;
                    send_world_cell_edit(sx, sy, sz, src_new_id, src_new_state, now_ms, src_fluid);
                    send_world_cell_edit(tx, ty, tz, dst_new_id, dst_new_state, now_ms, true);
                }
                return true;
            };

            const auto try_transfer = [&](io::i32 sx, io::i32 sy, io::i32 sz,
                                          ge::voxel::FluidKind moving_kind,
                                          io::i32 dx, io::i32 dy, io::i32 dz,
                                          io::u16 max_layers) noexcept -> bool {
                const io::i32 tx = sx + dx;
                const io::i32 ty = sy + dy;
                const io::i32 tz = sz + dz;
                ge::voxel::ChunkCoord dst_cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(tx, ty, tz, dst_cc, lx, ly, lz);
                if (!is_chunk_hot_any(dst_cc))
                    return false;
                const bool diagonal_path = (dy < 0 && (dx != 0 || dz != 0));
                return commit_transfer(sx, sy, sz, tx, ty, tz, moving_kind, max_layers, diagonal_path);
            };

            while (processed < process_budget && moved < move_budget) {
                WaterActiveCell cell{};
                if (!dequeue_water_cell(cell))
                    break;
                ++processed;

                ge::voxel::ChunkCoord cell_cc{};
                io::u32 lx = 0u, ly = 0u, lz = 0u;
                ge::voxel::split_world_coord(cell.wx, cell.wy, cell.wz, cell_cc, lx, ly, lz);
                if (!is_chunk_hot_any(cell_cc))
                    continue;

                ge::voxel::FluidStack src{};
                if (!load_stack(cell.wx, cell.wy, cell.wz, src))
                    continue;
                io::u8 src_total = ge::voxel::fluid_total_level(src);
                if (src_total == 0u)
                    continue;

                ge::voxel::FluidKind sink_kind = bottom_kind(src);
                ge::voxel::FluidKind float_kind = top_kind(src);
                if (sink_kind == ge::voxel::FluidKind::None && float_kind == ge::voxel::FluidKind::None)
                    continue;

                const io::u32 seed = static_cast<io::u32>((cell.wx * 73856093) ^ (cell.wy * 19349663) ^ (cell.wz * 83492791) ^ static_cast<io::i32>(now_ms & 0xFFFFu));
                const auto can_step = [&](ge::voxel::FluidKind kind, io::u32 salt) noexcept -> bool {
                    const FluidProfile p = profile_for(kind);
                    if (p.step_mod <= 1u) return true;
                    return ((seed + salt) % p.step_mod) == 0u;
                };

                const auto refresh_stack = [&](ge::voxel::FluidStack& st,
                                               io::u8& total,
                                               ge::voxel::FluidKind& out_sink,
                                               ge::voxel::FluidKind& out_float) noexcept -> bool {
                    if (!load_stack(cell.wx, cell.wy, cell.wz, st))
                        return false;
                    total = ge::voxel::fluid_total_level(st);
                    if (total == 0u)
                        return false;
                    out_sink = bottom_kind(st);
                    out_float = top_kind(st);
                    return out_sink != ge::voxel::FluidKind::None || out_float != ge::voxel::FluidKind::None;
                };

                const auto can_buoyancy_rise = [&](io::i32 wx, io::i32 wy, io::i32 wz,
                                                   ge::voxel::FluidKind moving_kind) noexcept -> bool {
                    if (moving_kind == ge::voxel::FluidKind::None)
                        return false;
                    const io::i32 tx = wx;
                    const io::i32 ty = wy + 1;
                    const io::i32 tz = wz;
                    ge::voxel::ChunkCoord up_cc{};
                    io::u32 ulx = 0u, uly = 0u, ulz = 0u;
                    ge::voxel::split_world_coord(tx, ty, tz, up_cc, ulx, uly, ulz);
                    if (!is_chunk_hot_any(up_cc))
                        return false;
                    if (hot_cell_is_collision(tx, ty, tz, now_ms))
                        return false;

                    ge::voxel::BlockId up_id = ge::voxel::BlockId::Air;
                    io::u16 up_state = 0u;
                    if (!try_get_world_state_for_water(tx, ty, tz, up_id, up_state))
                        return false;
                    if (!water_is_fillable(up_id))
                        return false;

                    ge::voxel::FluidStack up_stack = ge::voxel::fluid_stack_from_block(up_id, up_state);
                    ge::voxel::normalize_fluid_stack(up_stack);
                    if (ge::voxel::fluid_total_level(up_stack) >= ge::voxel::FLUID_LEVEL_MAX)
                        return false;

                    const io::u8 moving_density = ge::voxel::fluid_density(moving_kind);
                    const ge::voxel::FluidKind up_bottom = bottom_kind(up_stack);
                    const ge::voxel::FluidKind up_top = top_kind(up_stack);
                    if (up_bottom != ge::voxel::FluidKind::None &&
                        ge::voxel::fluid_density(up_bottom) > moving_density)
                        return true;
                    if (up_top != ge::voxel::FluidKind::None &&
                        ge::voxel::fluid_density(up_top) > moving_density)
                        return true;
                    return false;
                };

                bool moved_now = false;
                bool moved_sink = false;
                bool moved_float = false;
                FluidProfile sink_profile = profile_for(sink_kind);
                FluidProfile float_profile = profile_for(float_kind);
                bool has_distinct_top = false;

                if (!refresh_stack(src, src_total, sink_kind, float_kind))
                    continue;
                sink_profile = profile_for(sink_kind);
                float_profile = profile_for(float_kind);
                has_distinct_top =
                    src.top_kind != ge::voxel::FluidKind::None &&
                    src.top_level > 0u &&
                    src.top_kind != src.bottom_kind;

                if (!moved_now
                    && sink_kind != ge::voxel::FluidKind::None
                    && can_step(sink_kind, 3u)
                    && try_transfer(cell.wx, cell.wy, cell.wz, sink_kind, 0, -1, 0, sink_profile.down_max)) {
                    ++moved;
                    moved_now = true;
                    moved_sink = true;
                }

                if (!refresh_stack(src, src_total, sink_kind, float_kind))
                    continue;
                sink_profile = profile_for(sink_kind);
                float_profile = profile_for(float_kind);

                if (!moved_sink
                    && sink_profile.diag_max > 0u
                    && sink_kind != ge::voxel::FluidKind::None
                    && can_step(sink_kind, 7u)) {
                    static constexpr io::i32 DOWN_DIAG[8][2]{
                        { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 },
                        { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 }
                    };
                    const io::u16 src_kind = kind_level(src, sink_kind);
                    io::i32 best_dx = 0;
                    io::i32 best_dz = 0;
                    io::u16 best_kind = ge::voxel::FLUID_LEVEL_MAX;
                    io::u16 best_total = ge::voxel::FLUID_LEVEL_MAX;
                    bool have_best = false;
                    for (io::u32 i = 0u; i < 8u; ++i) {
                        const io::i32 dx = DOWN_DIAG[i][0];
                        const io::i32 dz = DOWN_DIAG[i][1];
                        const io::i32 tx = cell.wx + dx;
                        const io::i32 ty = cell.wy - 1;
                        const io::i32 tz = cell.wz + dz;
                        ge::voxel::BlockId nid = ge::voxel::BlockId::Air;
                        io::u16 nstate = 0u;
                        if (!try_get_world_state_for_water(tx, ty, tz, nid, nstate))
                            continue;
                        if (!water_is_fillable(nid))
                            continue;
                        if (hot_cell_is_collision(tx, ty, tz, now_ms))
                            continue;
                        const ge::voxel::FluidStack nstack = ge::voxel::fluid_stack_from_block(nid, nstate);
                        const io::u16 n_kind = kind_level(nstack, sink_kind);
                        if (src_kind <= static_cast<io::u16>(n_kind + 1u))
                            continue;
                        const io::u16 n_total = ge::voxel::fluid_total_level(nstack);
                        if (!have_best || n_kind < best_kind || (n_kind == best_kind && n_total < best_total)) {
                            best_kind = n_kind;
                            best_total = n_total;
                            best_dx = dx;
                            best_dz = dz;
                            have_best = true;
                        }
                    }
                    if (have_best && try_transfer(cell.wx, cell.wy, cell.wz, sink_kind, best_dx, -1, best_dz, sink_profile.diag_max)) {
                        ++moved;
                        moved_now = true;
                        moved_sink = true;
                    }
                }

                if (moved_sink
                    && sink_kind != ge::voxel::FluidKind::None
                    && ge::voxel::fluid_density(sink_kind) > ge::voxel::fluid_density(ge::voxel::FluidKind::Water)) {
                    for (io::u32 sub = 0u; sub < 2u && moved < move_budget; ++sub) {
                        if (!refresh_stack(src, src_total, sink_kind, float_kind))
                            break;
                        if (sink_kind == ge::voxel::FluidKind::None)
                            break;
                        if (!can_step(sink_kind, 23u + sub))
                            break;
                        if (!try_transfer(cell.wx, cell.wy, cell.wz, sink_kind, 0, -1, 0, 1u))
                            break;
                        ++moved;
                        moved_now = true;
                    }
                }

                if (!refresh_stack(src, src_total, sink_kind, float_kind))
                    continue;
                sink_profile = profile_for(sink_kind);
                float_profile = profile_for(float_kind);
                has_distinct_top =
                    src.top_kind != ge::voxel::FluidKind::None &&
                    src.top_level > 0u &&
                    src.top_kind != src.bottom_kind;

                if (!moved_sink
                    && sink_profile.side_max > 0u
                    && sink_kind != ge::voxel::FluidKind::None
                    && can_step(sink_kind, 13u)) {
                    static constexpr io::i32 SIDE[4][2]{
                        { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }
                    };
                    const io::u16 src_kind = kind_level(src, sink_kind);
                    io::i32 best_dx = 0;
                    io::i32 best_dz = 0;
                    io::u16 best_kind = ge::voxel::FLUID_LEVEL_MAX;
                    io::u16 best_total = ge::voxel::FLUID_LEVEL_MAX;
                    bool have_best = false;
                    for (io::u32 i = 0u; i < 4u; ++i) {
                        const io::i32 dx = SIDE[i][0];
                        const io::i32 dz = SIDE[i][1];
                        const io::i32 tx = cell.wx + dx;
                        const io::i32 ty = cell.wy;
                        const io::i32 tz = cell.wz + dz;
                        ge::voxel::BlockId nid = ge::voxel::BlockId::Air;
                        io::u16 nstate = 0u;
                        if (!try_get_world_state_for_water(tx, ty, tz, nid, nstate))
                            continue;
                        if (!water_is_fillable(nid))
                            continue;
                        if (hot_cell_is_collision(tx, ty, tz, now_ms))
                            continue;
                        const ge::voxel::FluidStack nstack = ge::voxel::fluid_stack_from_block(nid, nstate);
                        const io::u16 n_kind = kind_level(nstack, sink_kind);
                        if (src_kind <= static_cast<io::u16>(n_kind + 1u))
                            continue;
                        const io::u16 n_total = ge::voxel::fluid_total_level(nstack);
                        if (!have_best || n_kind < best_kind || (n_kind == best_kind && n_total < best_total)) {
                            best_kind = n_kind;
                            best_total = n_total;
                            best_dx = dx;
                            best_dz = dz;
                            have_best = true;
                        }
                    }
                    if (have_best && try_transfer(cell.wx, cell.wy, cell.wz, sink_kind, best_dx, 0, best_dz, sink_profile.side_max)) {
                        ++moved;
                        moved_now = true;
                        moved_sink = true;
                    }
                }

                if (!moved_float
                    && has_distinct_top
                    && float_kind != ge::voxel::FluidKind::None
                    && can_step(float_kind, 11u)
                    && can_buoyancy_rise(cell.wx, cell.wy, cell.wz, float_kind)
                    && try_transfer(cell.wx, cell.wy, cell.wz, float_kind, 0, 1, 0, 1u)) {
                    ++moved;
                    moved_now = true;
                    moved_float = true;
                }

                if (!refresh_stack(src, src_total, sink_kind, float_kind))
                    continue;
                sink_profile = profile_for(sink_kind);
                float_profile = profile_for(float_kind);
                has_distinct_top =
                    src.top_kind != ge::voxel::FluidKind::None &&
                    src.top_level > 0u &&
                    src.top_kind != src.bottom_kind;

                const bool allow_single_kind_side =
                    !has_distinct_top
                    && float_kind != ge::voxel::FluidKind::None
                    && src_total > 0u;

                if (!moved_float
                    && (has_distinct_top || allow_single_kind_side)
                    && float_profile.side_max > 0u
                    && float_kind != ge::voxel::FluidKind::None
                    && can_step(float_kind, 17u)) {
                    static constexpr io::i32 SIDE[4][2]{
                        { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }
                    };
                    io::u32 order4[4]{ 0u, 1u, 2u, 3u };
                    for (io::u32 i = 0u; i < 4u; ++i) {
                        const io::u32 j = (seed + i) & 3u;
                        const io::u32 t = order4[i];
                        order4[i] = order4[j];
                        order4[j] = t;
                    }

                    io::i32 best_dx = 0;
                    io::i32 best_dz = 0;
                    io::u16 best_level = ge::voxel::FLUID_LEVEL_MAX;
                    bool have_best = false;
                    for (io::u32 i = 0u; i < 4u; ++i) {
                        const io::i32 dx = SIDE[order4[i]][0];
                        const io::i32 dz = SIDE[order4[i]][1];
                        const io::i32 tx = cell.wx + dx;
                        const io::i32 ty = cell.wy;
                        const io::i32 tz = cell.wz + dz;
                        ge::voxel::BlockId nid = ge::voxel::BlockId::Air;
                        io::u16 nstate = 0u;
                        if (!try_get_world_state_for_water(tx, ty, tz, nid, nstate))
                            continue;
                        if (!water_is_fillable(nid))
                            continue;
                        if (hot_cell_is_collision(tx, ty, tz, now_ms))
                            continue;
                        const ge::voxel::FluidStack nstack = ge::voxel::fluid_stack_from_block(nid, nstate);
                        const io::u16 src_kind = kind_level(src, float_kind);
                        const io::u16 n_kind = kind_level(nstack, float_kind);
                        if (src_kind <= static_cast<io::u16>(n_kind + 1u))
                            continue;
                        if (!have_best || n_kind < best_level) {
                            best_level = n_kind;
                            best_dx = dx;
                            best_dz = dz;
                            have_best = true;
                        }
                    }
                    if (have_best && try_transfer(cell.wx, cell.wy, cell.wz, float_kind, best_dx, 0, best_dz, float_profile.side_max)) {
                        ++moved;
                        moved_now = true;
                        moved_float = true;
                    }
                }

                if (!refresh_stack(src, src_total, sink_kind, float_kind))
                    continue;
                sink_profile = profile_for(sink_kind);
                float_profile = profile_for(float_kind);
                has_distinct_top =
                    src.top_kind != ge::voxel::FluidKind::None &&
                    src.top_level > 0u &&
                    src.top_kind != src.bottom_kind;
                const bool allow_single_kind_side_down =
                    !has_distinct_top
                    && float_kind != ge::voxel::FluidKind::None
                    && src_total > 0u;
                if (!moved_float
                    && (has_distinct_top || allow_single_kind_side_down)
                    && float_profile.diag_max > 0u
                    && float_kind != ge::voxel::FluidKind::None
                    && can_step(float_kind, 31u)) {
                    static constexpr io::i32 DOWN_DIAG[8][2]{
                        { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 },
                        { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 }
                    };
                    const io::u16 src_kind = kind_level(src, float_kind);
                    io::i32 best_dx = 0;
                    io::i32 best_dz = 0;
                    io::u16 best_kind = ge::voxel::FLUID_LEVEL_MAX;
                    io::u16 best_total = ge::voxel::FLUID_LEVEL_MAX;
                    bool have_best = false;
                    for (io::u32 i = 0u; i < 8u; ++i) {
                        const io::i32 dx = DOWN_DIAG[i][0];
                        const io::i32 dz = DOWN_DIAG[i][1];
                        const io::i32 tx = cell.wx + dx;
                        const io::i32 ty = cell.wy - 1;
                        const io::i32 tz = cell.wz + dz;
                        ge::voxel::BlockId nid = ge::voxel::BlockId::Air;
                        io::u16 nstate = 0u;
                        if (!try_get_world_state_for_water(tx, ty, tz, nid, nstate))
                            continue;
                        if (!water_is_fillable(nid))
                            continue;
                        if (hot_cell_is_collision(tx, ty, tz, now_ms))
                            continue;
                        const ge::voxel::FluidStack nstack = ge::voxel::fluid_stack_from_block(nid, nstate);
                        const io::u16 n_kind = kind_level(nstack, float_kind);
                        if (src_kind <= static_cast<io::u16>(n_kind + 1u))
                            continue;
                        const io::u16 n_total = ge::voxel::fluid_total_level(nstack);
                        if (!have_best || n_kind < best_kind || (n_kind == best_kind && n_total < best_total)) {
                            best_kind = n_kind;
                            best_total = n_total;
                            best_dx = dx;
                            best_dz = dz;
                            have_best = true;
                        }
                    }
                    if (have_best && try_transfer(cell.wx, cell.wy, cell.wz, float_kind, best_dx, -1, best_dz, 1u)) {
                        ++moved;
                        moved_now = true;
                        moved_float = true;
                    }
                }

                if (!moved_now) {
                    const bool has_mixed_kinds =
                        sink_kind != ge::voxel::FluidKind::None &&
                        float_kind != ge::voxel::FluidKind::None &&
                        sink_kind != float_kind;
                    const bool partial_cell = src_total > 0u && src_total < ge::voxel::FLUID_LEVEL_MAX;
                    if (partial_cell || has_mixed_kinds || ((seed & 1u) == 0u))
                        enqueue_water_cell(cell.wx, cell.wy, cell.wz, now_ms);
                }
            }

            for (io::u32 i = 0u; i < touched_count; ++i) {
                ge::voxel::ChunkData* c = world.find_chunk(water_touched[i]);
                if (!c) continue;
                c->generated = true;
                c->dirty_mesh = false;
                c->dirty_neighbors = false;
                (void)ge::voxel::save_chunk_binary(*c);
            }
        }
        IO_NODISCARD inline bool broadcast_block_edit_ex(const ge::net::BlockEdit& edit,
                                                         io::u64 now_ms,
                                                         bool prefer_unreliable,
                                                         bool queue_on_fail = true) noexcept {
            if (!peers) return true;
            ge::net::S2C_BlockEdit wire{};
            ge::net::encode_s2c_block_edit(edit, wire);
            bool all_ok = true;
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used) continue;
                bool ok = false;
                if (prefer_unreliable) {
                    ok = loop.send_to_peer(
                        p.ep,
                        ge::net::PK_S2C_BLOCK_EDIT,
                        io::UdpChan::Unreliable,
                        io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                        now_ms);
                    if (!ok) {
                        note_packet_backpressure();
                        ok = loop.send_to_peer(
                            p.ep,
                            ge::net::PK_S2C_BLOCK_EDIT,
                            io::UdpChan::Reliable,
                            io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                            now_ms);
                    }
                } else {
                    ok = loop.send_to_peer(
                        p.ep,
                        ge::net::PK_S2C_BLOCK_EDIT,
                        io::UdpChan::Reliable,
                        io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                        now_ms);
                }
                if (ok) {
                    ++stats.send_ok;
                } else {
                    note_packet_backpressure();
                    ++stats.send_fail;
                    all_ok = false;
                }
            }
            if (!all_ok && queue_on_fail)
                enqueue_deferred_block_edit(edit, prefer_unreliable);
            return all_ok;
        }

        inline void broadcast_block_edit(const ge::net::BlockEdit& edit, io::u64 now_ms) noexcept {
            (void)broadcast_block_edit_ex(edit, now_ms, false);
        }

        inline void flush_deferred_block_edits(io::u64 now_ms) noexcept {
            if (!deferred_block_edits || deferred_block_edit_count == 0u) return;
            io::u32 budget = BLOCK_EDIT_DEFER_FLUSH_BUDGET;
            while (budget-- > 0u && deferred_block_edit_count > 0u) {
                const DeferredBlockEdit& item = deferred_block_edits[deferred_block_edit_head];
                if (!broadcast_block_edit_ex(item.edit, now_ms, item.prefer_unreliable, false))
                    break;
                deferred_block_edit_head = (deferred_block_edit_head + 1u) % BLOCK_EDIT_DEFER_CAP;
                --deferred_block_edit_count;
            }
        }

        static inline io::char_view peer_fallback_name(io::u16 peer_index, io::StackOut<32>& out) noexcept {
            out.reset();
            out << "peer#" << static_cast<io::u32>(peer_index);
            return out.view();
        }

        static inline void fill_chat_line(ge::net::ChatLine& out,
                                          io::u8 kind,
                                          io::char_view name,
                                          io::char_view text) noexcept {
            out = {};
            out.kind = kind;

            io::usize name_n = name.size();
            if (name_n > ge::net::CHAT_NAME_MAX) name_n = ge::net::CHAT_NAME_MAX;
            out.name_len = static_cast<io::u8>(name_n);
            for (io::u32 i = 0; i < ge::net::CHAT_NAME_MAX; ++i) {
                char ch = (i < out.name_len) ? name[i] : '\0';
                if (ch == '\n' || ch == '\r') ch = ' ';
                out.name[i] = ch;
            }

            const io::char_view trimmed = trim_view(text);
            io::usize text_n = trimmed.size();
            if (text_n > ge::net::CHAT_TEXT_MAX) text_n = ge::net::CHAT_TEXT_MAX;
            out.text_len = static_cast<io::u8>(text_n);
            for (io::u32 i = 0; i < ge::net::CHAT_TEXT_MAX; ++i) {
                char ch = (i < out.text_len) ? trimmed[i] : '\0';
                if (ch == '\n' || ch == '\r') ch = ' ';
                out.text[i] = ch;
            }
        }

        IO_NODISCARD inline bool send_chat_to_peer(io::u16 peer_index, const ge::net::ChatLine& line, io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            const PeerState& p = peers[peer_index];
            if (!p.used) return false;
            ge::net::S2C_Chat wire{};
            ge::net::encode_s2c_chat(line, wire);
            bool ok = loop.send_to_peer(
                p.ep,
                ge::net::PK_S2C_CHAT,
                io::UdpChan::Reliable,
                io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                now_ms);
            if (!ok) {
                note_packet_backpressure();
                // Chat is low-bandwidth but user-visible; fallback to unreliable delivery
                // when reliable window is currently full.
                ok = loop.send_to_peer(
                    p.ep,
                    ge::net::PK_S2C_CHAT,
                    io::UdpChan::Unreliable,
                    io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                    now_ms);
            }
            if (ok) {
                ++stats.send_ok;
                ++stats.chat_tx;
            } else {
                ++stats.send_fail;
            }
            return ok;
        }

        inline void send_system_chat_to_peer(io::u16 peer_index, io::char_view text, io::u64 now_ms) noexcept {
            ge::net::ChatLine msg{};
            fill_chat_line(msg, ge::net::CHAT_KIND_SERVER, "SERVER", text);
            (void)send_chat_to_peer(peer_index, msg, now_ms);
        }

        inline void broadcast_chat(const ge::net::ChatLine& line, io::u64 now_ms) noexcept {
            if (!peers) return;
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                if (!peers[i].used) continue;
                (void)send_chat_to_peer(i, line, now_ms);
            }
        }

        IO_NODISCARD static inline bool parse_block_id_token(io::char_view tok, ge::voxel::BlockId& out) noexcept {
            tok = trim_view(tok);
            if (tok.empty()) return false;
            io::i32 numeric = 0;
            if (parse_i32_token(tok, numeric)) {
                if (numeric < 0 || numeric >= static_cast<io::i32>(ge::voxel::BLOCK_COUNT)) return false;
                out = static_cast<ge::voxel::BlockId>(numeric);
                return true;
            }
            if (eq_icase(tok, "air"))   { out = ge::voxel::BlockId::Air; return true; }
            if (eq_icase(tok, "grass")) { out = ge::voxel::BlockId::Grass; return true; }
            if (eq_icase(tok, "dirt"))  { out = ge::voxel::BlockId::Dirt; return true; }
            if (eq_icase(tok, "stone")) { out = ge::voxel::BlockId::Stone; return true; }
            if (eq_icase(tok, "sand"))  { out = ge::voxel::BlockId::Sand; return true; }
            if (eq_icase(tok, "water")) { out = ge::voxel::BlockId::Water; return true; }
            if (eq_icase(tok, "waterdark") || eq_icase(tok, "water_dark") || eq_icase(tok, "water-dark")) {
                out = ge::voxel::BlockId::WaterDark; return true;
            }
            if (eq_icase(tok, "blood")) { out = ge::voxel::BlockId::Blood; return true; }
            if (eq_icase(tok, "blooddark") || eq_icase(tok, "blood_dark") || eq_icase(tok, "blood-dark")) {
                out = ge::voxel::BlockId::BloodDark; return true;
            }
            if (eq_icase(tok, "slime")) { out = ge::voxel::BlockId::Slime; return true; }
            if (eq_icase(tok, "slimedark") || eq_icase(tok, "slime_dark") || eq_icase(tok, "slime-dark")) {
                out = ge::voxel::BlockId::SlimeDark; return true;
            }
            if (eq_icase(tok, "snow"))  { out = ge::voxel::BlockId::Snow; return true; }
            if (eq_icase(tok, "book"))  { out = ge::voxel::BlockId::LevitatingBookAnchor; return true; }
            return false;
        }

        IO_NODISCARD static inline bool parse_item_id_token(io::char_view tok, ge::item::Id& out) noexcept {
            tok = trim_view(tok);
            if (tok.empty()) return false;
            io::i32 numeric = 0;
            if (parse_i32_token(tok, numeric)) {
                if (numeric < 0 || numeric >= static_cast<io::i32>(ge::item::ITEM_COUNT)) return false;
                out = static_cast<ge::item::Id>(numeric);
                return true;
            }
            if (eq_icase(tok, "grass") || eq_icase(tok, "grass_block")) { out = ge::item::Id::GrassBlock; return true; }
            if (eq_icase(tok, "dirt") || eq_icase(tok, "dirt_block")) { out = ge::item::Id::DirtBlock; return true; }
            if (eq_icase(tok, "stone") || eq_icase(tok, "stone_block")) { out = ge::item::Id::StoneBlock; return true; }
            if (eq_icase(tok, "sand") || eq_icase(tok, "sand_block")) { out = ge::item::Id::SandBlock; return true; }
            if (eq_icase(tok, "log") || eq_icase(tok, "log_block")) { out = ge::item::Id::LogBlock; return true; }
            if (eq_icase(tok, "leaves") || eq_icase(tok, "leaves_block")) { out = ge::item::Id::LeavesBlock; return true; }
            if (eq_icase(tok, "potato")) { out = ge::item::Id::Potato; return true; }
            if (eq_icase(tok, "spellward") || eq_icase(tok, "spell_ward") || eq_icase(tok, "spell-ward")) {
                out = ge::item::Id::SpellWard; return true;
            }
            if (eq_icase(tok, "spellbolt") || eq_icase(tok, "spell_bolt") || eq_icase(tok, "spell-bolt") || eq_icase(tok, "bolt")) {
                out = ge::item::Id::SpellBolt; return true;
            }
            if (eq_icase(tok, "spelldig") || eq_icase(tok, "spell_dig") || eq_icase(tok, "spell-dig") || eq_icase(tok, "dig")) {
                out = ge::item::Id::SpellDig; return true;
            }
            if (eq_icase(tok, "spellburst") || eq_icase(tok, "spell_burst") || eq_icase(tok, "spell-burst") || eq_icase(tok, "burst")) {
                out = ge::item::Id::SpellBurst; return true;
            }
            if (eq_icase(tok, "spellbeam") || eq_icase(tok, "spell_beam") || eq_icase(tok, "spell-beam") || eq_icase(tok, "beam")) {
                out = ge::item::Id::SpellBeam; return true;
            }
            if (eq_icase(tok, "spellorb") || eq_icase(tok, "spell_orb") || eq_icase(tok, "spell-orb") || eq_icase(tok, "orb")) {
                out = ge::item::Id::SpellOrb; return true;
            }
            if (eq_icase(tok, "spellmine") || eq_icase(tok, "spell_mine") || eq_icase(tok, "spell-mine") || eq_icase(tok, "mine")) {
                out = ge::item::Id::SpellMine; return true;
            }
            if (eq_icase(tok, "spellshieldpulse") || eq_icase(tok, "spell_shield_pulse") ||
                eq_icase(tok, "spell-shield-pulse") || eq_icase(tok, "shieldpulse") || eq_icase(tok, "shield_pulse")) {
                out = ge::item::Id::SpellShieldPulse; return true;
            }
            if (eq_icase(tok, "spellmark") || eq_icase(tok, "spell_mark") || eq_icase(tok, "spell-mark") || eq_icase(tok, "mark")) {
                out = ge::item::Id::SpellMark; return true;
            }
            if (eq_icase(tok, "spellpull") || eq_icase(tok, "spell_pull") || eq_icase(tok, "spell-pull") || eq_icase(tok, "pull")) {
                out = ge::item::Id::SpellPull; return true;
            }
            if (eq_icase(tok, "spellblinkstep") || eq_icase(tok, "spell_blink_step") ||
                eq_icase(tok, "spell-blink-step") || eq_icase(tok, "blinkstep") || eq_icase(tok, "blink_step")) {
                out = ge::item::Id::SpellBlinkStep; return true;
            }
            if (eq_icase(tok, "dagger") || eq_icase(tok, "rusty_dagger") || eq_icase(tok, "rusty-dagger")) {
                out = ge::item::Id::RustyDagger; return true;
            }
            return false;
        }

        IO_NODISCARD static inline bool parse_on_off_token(io::char_view tok, bool& out_enabled) noexcept {
            tok = trim_view(tok);
            if (tok.empty()) return false;
            if (eq_icase(tok, "on") || eq_icase(tok, "1") || eq_icase(tok, "true") || eq_icase(tok, "start")) {
                out_enabled = true;
                return true;
            }
            if (eq_icase(tok, "off") || eq_icase(tok, "0") || eq_icase(tok, "false") || eq_icase(tok, "stop")) {
                out_enabled = false;
                return true;
            }
            return false;
        }

        inline void run_chat_command(io::u16 peer_index, io::char_view full_text, io::u64 now_ms) noexcept {
            io::char_view tail = trim_view(full_text);
            const io::char_view command = pop_token(tail);
            if (command.empty()) {
                send_system_chat_to_peer(peer_index, "Empty command. Use /help", now_ms);
                return;
            }

            if (eq_icase(command, "/help")) {
                send_system_chat_to_peer(peer_index, "Commands: /help, /give @name item, /pos, /tp x y z, /setblock x y z block, /watersource x y z on|off, /settime 1..100, /godmode, /noclip, /region, /regionneighbors, /regionset, /regionrepair, /regionstep", now_ms);
                return;
            }

            if (eq_icase(command, "/give")) {
                const io::char_view target = pop_token(tail);
                const io::char_view item_tok = pop_token(tail);
                if (target.empty() || item_tok.empty() || target[0] != '@') {
                    send_system_chat_to_peer(peer_index, "Usage: /give @nickname item_name", now_ms);
                    return;
                }
                const io::char_view nickname = target.slice(1u, target.size() - 1u);
                const io::i32 target_peer = find_peer_by_name(nickname);
                if (target_peer < 0) {
                    send_system_chat_to_peer(peer_index, "Target player not found", now_ms);
                    return;
                }
                ge::item::Id item_id = ge::item::Id::None;
                if (!parse_item_id_token(item_tok, item_id) || item_id == ge::item::Id::None) {
                    send_system_chat_to_peer(peer_index, "Unknown item for /give", now_ms);
                    return;
                }
                ge::item::Stack stack = ge::item::make_stack(item_id, 1u);
                if (!give_item_to_peer(static_cast<io::u16>(target_peer), stack, now_ms)) {
                    send_system_chat_to_peer(peer_index, "Target inventory is full", now_ms);
                    return;
                }
                io::StackOut<128> ss{};
                ss << "Gave " << ge::item::name(item_id) << " to @" << nickname;
                send_system_chat_to_peer(peer_index, ss.view(), now_ms);
                return;
            }

            if (eq_icase(command, "/respawn")) {
                send_system_chat_to_peer(peer_index, "Respawn is automatic now", now_ms);
                return;
            }

            if (eq_icase(command, "/godmode")) {
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used) {
                    send_system_chat_to_peer(peer_index, "Peer is not valid", now_ms);
                    return;
                }
                PeerState& p = peers[peer_index];
                p.use_fly = !p.use_fly;
                sync_player_ecs_from_peer(peer_index);
                send_system_chat_to_peer(peer_index, p.use_fly ? "godmode enabled" : "godmode disabled", now_ms);
                return;
            }

            if (eq_icase(command, "/noclip")) {
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used) {
                    send_system_chat_to_peer(peer_index, "Peer is not valid", now_ms);
                    return;
                }
                PeerState& p = peers[peer_index];
                p.use_noclip = !p.use_noclip;
                sync_player_ecs_from_peer(peer_index);
                send_system_chat_to_peer(peer_index, p.use_noclip ? "noclip enabled" : "noclip disabled", now_ms);
                return;
            }

            if (eq_icase(command, "/settime")) {
                const io::char_view tval = pop_token(tail);
                io::i32 value = 0;
                if (!parse_i32_token(tval, value) || value < 1 || value > 100) {
                    send_system_chat_to_peer(peer_index, "Usage: /settime 1..100", now_ms);
                    return;
                }
                const io::u32 phase_ms = SetTimeValueToPhaseMs(value);
                SetWorldPhaseMs(phase_ms, now_ms);
                if (peers) {
                    for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                        if (!peers[i].used) continue;
                        (void)send_world_time_to_peer(i, now_ms, io::UdpChan::Reliable);
                    }
                }
                io::StackOut<96> ss{};
                ss << "time set to " << static_cast<io::u32>(value);
                send_system_chat_to_peer(peer_index, ss.view(), now_ms);
                return;
            }

            if (eq_icase(command, "/pos")) {
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used || !peers[peer_index].has_auth) {
                    send_system_chat_to_peer(peer_index, "Position is not available yet", now_ms);
                    return;
                }
                float pos_x = peers[peer_index].auth_x;
                float pos_y = peers[peer_index].auth_y;
                float pos_z = peers[peer_index].auth_z;
                if (player_ecs && player_ecs->alive[peer_index] != 0u) {
                    pos_x = player_ecs->transform[peer_index].x;
                    pos_y = player_ecs->transform[peer_index].y;
                    pos_z = player_ecs->transform[peer_index].z;
                }
                io::StackOut<96> ss{};
                ss << "pos: x=" << floor_to_i32(pos_x)
                   << " y=" << floor_to_i32(pos_y)
                   << " z=" << floor_to_i32(pos_z);
                send_system_chat_to_peer(peer_index, ss.view(), now_ms);
                return;
            }

            if (eq_icase(command, "/tp")) {
                const io::char_view tx = pop_token(tail);
                const io::char_view ty = pop_token(tail);
                const io::char_view tz = pop_token(tail);
                io::i32 x = 0, y = 0, z = 0;
                if (!parse_i32_token(tx, x) || !parse_i32_token(ty, y) || !parse_i32_token(tz, z)) {
                    send_system_chat_to_peer(peer_index, "Usage: /tp x y z", now_ms);
                    return;
                }
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used) {
                    send_system_chat_to_peer(peer_index, "Peer is not valid", now_ms);
                    return;
                }
                PeerState& p = peers[peer_index];
                p.auth_x = static_cast<float>(x);
                p.auth_y = static_cast<float>(y);
                p.auth_z = static_cast<float>(z);
                p.auth_ms = now_ms;
                p.has_auth = true;
                p.has_pending = false;
                p.grounded = is_player_grounded(p.auth_x, p.auth_y, p.auth_z, p.crawling);
                p.airborne = !p.grounded;
                p.airborne_ms = 0u;
                p.air_peak_foot_y = p.auth_y - player_eye_to_feet_for_mode(p.crawling);
                p.stream_center_valid = false;
                sync_player_ecs_from_peer(peer_index);
                (void)send_pos(peer_index, now_ms, ge::net::PLAYER_POS_FLAG_CORRECTION, io::UdpChan::Reliable);
                send_system_chat_to_peer(peer_index, "Teleported", now_ms);
                return;
            }

            if (eq_icase(command, "/setblock")) {
                const io::char_view tx = pop_token(tail);
                const io::char_view ty = pop_token(tail);
                const io::char_view tz = pop_token(tail);
                const io::char_view tid = pop_token(tail);
                io::i32 x = 0, y = 0, z = 0;
                ge::voxel::BlockId block_id = ge::voxel::BlockId::Air;
                if (!parse_i32_token(tx, x) || !parse_i32_token(ty, y) || !parse_i32_token(tz, z) || !parse_block_id_token(tid, block_id)) {
                    send_system_chat_to_peer(peer_index, "Usage: /setblock x y z block (name or id)", now_ms);
                    return;
                }
                ge::net::BlockEdit edit{};
                edit.wx = x;
                edit.wy = y;
                edit.wz = z;
                edit.block_id = ge::voxel::block_index(block_id);
                edit.state = 0u;
                if (!apply_block_edit_world(edit)) {
                    send_system_chat_to_peer(peer_index, "setblock failed (chunk not loaded yet)", now_ms);
                    return;
                }
                // Explicit wake-up for fluid placement via command.
                if (ge::voxel::is_fluid_block_id(block_id))
                    enqueue_water_neighborhood(x, y, z, now_ms);
                ++stats.block_edits_ok;
                broadcast_block_edit(edit, now_ms);
                send_system_chat_to_peer(peer_index, "setblock done", now_ms);
                return;
            }

            if (eq_icase(command, "/watersource")) {
                const io::char_view tx = pop_token(tail);
                const io::char_view ty = pop_token(tail);
                const io::char_view tz = pop_token(tail);
                const io::char_view tenable = pop_token(tail);
                io::i32 x = 0, y = 0, z = 0;
                bool enabled = false;
                if (!parse_i32_token(tx, x) || !parse_i32_token(ty, y) || !parse_i32_token(tz, z) || !parse_on_off_token(tenable, enabled)) {
                    send_system_chat_to_peer(peer_index, "Usage: /watersource x y z on|off", now_ms);
                    return;
                }

                ge::net::BlockEdit edit{};
                edit.wx = x;
                edit.wy = y;
                edit.wz = z;
                if (enabled) {
                    edit.block_id = ge::voxel::block_index(ge::voxel::BlockId::Water);
                    edit.state = 0u;
                } else {
                    edit.block_id = ge::voxel::block_index(ge::voxel::BlockId::Air);
                    edit.state = 0u;
                }

                if (!apply_block_edit_world(edit)) {
                    send_system_chat_to_peer(peer_index, "watersource failed (chunk not loaded yet)", now_ms);
                    return;
                }
                ++stats.block_edits_ok;
                broadcast_block_edit(edit, now_ms);
                send_system_chat_to_peer(peer_index, enabled ? "water started" : "water stopped", now_ms);
                return;
            }

            if (eq_icase(command, "/region")) {
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used || !peers[peer_index].has_auth) {
                    send_system_chat_to_peer(peer_index, "Region is not available yet", now_ms);
                    return;
                }
                const PeerState& p = peers[peer_index];
                RegionStateSlot snapshot{};
                if (!region_debug_get_for_peer(peer_index, snapshot)) {
                    send_system_chat_to_peer(peer_index, "Region lookup failed", now_ms);
                    return;
                }
                const io::i32 border = region_border_distance_blocks(floor_to_i32(p.auth_x), floor_to_i32(p.auth_z));
                io::StackOut<240> ss{};
                ss << "region id=" << ge::net::region_id_hi(snapshot.id) << ":" << ge::net::region_id_lo(snapshot.id)
                   << " cell=(" << snapshot.coord.cell_x << "," << snapshot.coord.band_y << "," << snapshot.coord.cell_z << ")"
                   << " mana=" << static_cast<io::u32>(snapshot.mana)
                   << " instability=" << static_cast<io::u32>(snapshot.instability)
                   << " decay=" << static_cast<io::u32>(snapshot.decay)
                   << " bands(m/i/d)="
                   << static_cast<io::u32>(snapshot.mana_band) << "/"
                   << static_cast<io::u32>(snapshot.instability_band) << "/"
                   << static_cast<io::u32>(snapshot.decay_band)
                   << " border~" << border;
                send_system_chat_to_peer(peer_index, ss.view(), now_ms);
                return;
            }

            if (eq_icase(command, "/regionneighbors")) {
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used || !peers[peer_index].has_auth) {
                    send_system_chat_to_peer(peer_index, "Region is not available yet", now_ms);
                    return;
                }
                RegionStateSlot snapshot{};
                if (!region_debug_get_for_peer(peer_index, snapshot)) {
                    send_system_chat_to_peer(peer_index, "Region lookup failed", now_ms);
                    return;
                }
                io::StackOut<320> ss{};
                ss << "region neighbors:";
                for (io::u32 i = 0u; i < ge::region::NEIGHBOR_COUNT; ++i) {
                    const ge::region::RegionId id = snapshot.neighbors[i];
                    ss << " [" << i << "] "
                       << ge::net::region_id_hi(id) << ":" << ge::net::region_id_lo(id);
                }
                send_system_chat_to_peer(peer_index, ss.view(), now_ms);
                return;
            }

            if (eq_icase(command, "/regionset")) {
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used || !peers[peer_index].has_auth) {
                    send_system_chat_to_peer(peer_index, "Peer is not valid", now_ms);
                    return;
                }
                const io::char_view prop = pop_token(tail);
                const io::char_view tok_value = pop_token(tail);
                io::i32 value = 0;
                if (prop.empty() || !parse_i32_token(tok_value, value)) {
                    send_system_chat_to_peer(peer_index, "Usage: /regionset mana|instability|decay delta", now_ms);
                    return;
                }
                PeerState& p = peers[peer_index];
                RegionStateSlot* slot = region_ensure_by_world(floor_to_i32(p.auth_x), floor_to_i32(p.auth_y), floor_to_i32(p.auth_z), now_ms);
                if (!slot) {
                    send_system_chat_to_peer(peer_index, "Region lookup failed", now_ms);
                    return;
                }
                io::i32 dm = 0, di = 0, dd = 0;
                if (eq_icase(prop, "mana")) dm = value;
                else if (eq_icase(prop, "instability")) di = value;
                else if (eq_icase(prop, "decay")) dd = value;
                else {
                    send_system_chat_to_peer(peer_index, "regionset property: mana | instability | decay", now_ms);
                    return;
                }
                region_apply_delta(*slot, dm, di, dd, now_ms);
                region_save_needed = true;
                region_next_sync_ms = 0u;
                region_sync_peers(now_ms, io::UdpChan::Reliable);
                send_system_chat_to_peer(peer_index, "regionset applied", now_ms);
                return;
            }

            if (eq_icase(command, "/regionstep")) {
                io::i32 steps = 1;
                const io::char_view tok_steps = pop_token(tail);
                if (!tok_steps.empty() && !parse_i32_token(tok_steps, steps)) {
                    send_system_chat_to_peer(peer_index, "Usage: /regionstep [1..16]", now_ms);
                    return;
                }
                if (steps < 1) steps = 1;
                if (steps > 16) steps = 16;
                for (io::i32 i = 0; i < steps; ++i)
                    region_diffusion_step(now_ms, true);
                region_next_sync_ms = 0u;
                region_sync_peers(now_ms, io::UdpChan::Reliable);
                region_save_needed = true;
                send_system_chat_to_peer(peer_index, "region diffusion step executed", now_ms);
                return;
            }

            if (eq_icase(command, "/regionrepair")) {
                if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS) || !peers[peer_index].used || !peers[peer_index].has_auth) {
                    send_system_chat_to_peer(peer_index, "Region is not available yet", now_ms);
                    return;
                }
                const PeerState& p = peers[peer_index];
                region_note_memory_repair(floor_to_i32(p.auth_x), floor_to_i32(p.auth_y), floor_to_i32(p.auth_z), now_ms);
                region_next_sync_ms = 0u;
                region_sync_peers(now_ms, io::UdpChan::Reliable);
                send_system_chat_to_peer(peer_index, "region memory-repair applied", now_ms);
                return;
            }

            send_system_chat_to_peer(peer_index, "Unknown command. Use /help", now_ms);
        }

        IO_NODISCARD inline io::u16 find_peer(io::Endpoint ep) const noexcept {
            if (!peers) return INVALID_PEER;
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                if (!peers[i].used) continue;
                if (endpoint_eq(peers[i].ep, ep)) return i;
            }
            return INVALID_PEER;
        }

        inline void remove_pending_for_peer(io::u16 peer_index) noexcept {
            if (!pending || pending_count == 0u) return;
            io::u32 dst = 0u;
            for (io::u32 i = 0; i < pending_count; ++i) {
                const io::u32 src_idx = (pending_head + i) % MAX_PENDING;
                const PendingReq req = pending[src_idx];
                if (req.peer_index == peer_index) {
                    if (req.stream_slot != INVALID_STREAM_SLOT) {
                        stream_set(req.peer_index, req.stream_slot, StreamUnsent);
                        stream_set_reqid(req.peer_index, req.stream_slot, 0u);
                        stream_set_sent_ms(req.peer_index, req.stream_slot, 0u);
                    }
                    continue;
                }
                const io::u32 dst_idx = (pending_head + dst) % MAX_PENDING;
                if (dst_idx != src_idx)
                    pending[dst_idx] = req;
                ++dst;
            }
            pending_count = dst;
            pending_tail = (pending_head + dst) % MAX_PENDING;
        }

        inline void cancel_slots_for_peer(io::u16 peer_index) noexcept {
            if (!worker_slots) return;
            for (io::u32 i = 0; i < worker.slot_count; ++i) {
                WorkerSlot& slot = worker_slots[i];
                const io::u32 st = slot.state.load(io::memory_order_acquire);
                if (st == SlotFree) continue;
                if (slot.job.peer_index != peer_index) continue;
                slot.cancel.store(1u, io::memory_order_release);
                if (st == SlotReady) {
                    slot.cancel.store(0u, io::memory_order_release);
                    clear_slot_backpressure_latch(slot);
                    slot.task = WorkerTask::None;
                    slot.state.store(SlotFree, io::memory_order_release);
                    if (slot.job.stream_slot != INVALID_STREAM_SLOT) {
                        stream_set(slot.job.peer_index, slot.job.stream_slot, StreamUnsent);
                        stream_set_reqid(slot.job.peer_index, slot.job.stream_slot, 0u);
                        stream_set_sent_ms(slot.job.peer_index, slot.job.stream_slot, 0u);
                    }
                    if (stats.jobs_inflight > 0u) --stats.jobs_inflight;
                    ++stats.jobs_canceled;
                }
            }
        }

        inline void cleanup_peer(io::u16 peer_index) noexcept {
            remove_pending_for_peer(peer_index);
            cancel_slots_for_peer(peer_index);
        }

        inline void drop_peer(io::u16 peer_index) noexcept {
            if (!peers || peer_index == INVALID_PEER) return;
            io::char_view leaving_name{};
            io::StackOut<32> fallback_name{};
            if (peers[peer_index].used) {
                if (peers[peer_index].name_len > 0u)
                    leaving_name = io::char_view{ peers[peer_index].name_utf8, peers[peer_index].name_len };
                else
                    leaving_name = peer_fallback_name(peer_index, fallback_name);
            }
            const io::u64 now_ms = io::monotonic_ms();
            if (!leaving_name.empty()) {
                io::StackOut<192> msg{};
                msg << leaving_name << " left the game";
                ge::net::ChatLine out{};
                fill_chat_line(out, ge::net::CHAT_KIND_SERVER, "SERVER", msg.view());
                broadcast_chat(out, now_ms);
            }
            broadcast_roster_remove(peer_index, now_ms);
            cleanup_peer(peer_index);
            stream_reset(peer_index);
            if (player_ecs) player_ecs->Deactivate(peer_index);
            peers[peer_index] = {};
            peers[peer_index].next_request_id = 1u;
        }

        inline void drop_peer_by_endpoint(io::Endpoint ep) noexcept {
            const io::u16 idx = find_peer(ep);
            if (idx != INVALID_PEER) drop_peer(idx);
        }

        IO_NODISCARD inline bool init(io::u32 wanted_threads,
                                      io::u32 wanted_distance,
                                      io::u32 wanted_hot_chunks,
                                      io::u32 wanted_day_time_multiply) noexcept {
            if (wanted_threads == 0u) wanted_threads = 1u;
            worker.threads = wanted_threads;
            if (wanted_distance < 1u) wanted_distance = 1u;
            if (wanted_distance > 16u) wanted_distance = 16u;
            stream.distance = wanted_distance;
            if (wanted_hot_chunks < 1u) wanted_hot_chunks = 1u;
            if (wanted_hot_chunks > 16u) wanted_hot_chunks = 16u;
            if (wanted_hot_chunks > stream.distance) wanted_hot_chunks = stream.distance;
            stream.hot_chunks = wanted_hot_chunks;
            if (wanted_day_time_multiply < 1u) wanted_day_time_multiply = 1u;
            if (wanted_day_time_multiply > 240u) wanted_day_time_multiply = 240u;
            world_time.day_ms = 60000u * wanted_day_time_multiply;
            world_time.night_ms = 45000u * wanted_day_time_multiply;
            world_time.epoch_ms = io::monotonic_ms();
            world_time.next_broadcast_ms = 0u;
            if (!ge::build::ensure_block_build_profile(block_build_profile))
                ge::build::set_default_block_build_profile(block_build_profile);

            recv_buf_mem.reset(static_cast<io::u8*>(io::alloc_aligned(RECV_BUF_CAP, alignof(io::u8))));
            if (!recv_buf_mem.get()) return false;
            recv_buf = recv_buf_mem.get();
            for (io::u32 i = 0; i < RECV_BUF_CAP; ++i) recv_buf[i] = 0u;

            part_buf_mem.reset(static_cast<io::u8*>(io::alloc_aligned(PART_BUF_CAP, alignof(io::u8))));
            if (!part_buf_mem.get()) return false;
            part_buf = part_buf_mem.get();
            for (io::u32 i = 0; i < PART_BUF_CAP; ++i) part_buf[i] = 0u;

            terrain_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(ge::worldgen::SimplexTerrain), alignof(ge::worldgen::SimplexTerrain))));
            if (!terrain_mem.get()) return false;
            terrain = new (terrain_mem.get()) ge::worldgen::SimplexTerrain{ 424242u };
            if (!world.init(0u)) return false;

            pending_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(PendingReq) * MAX_PENDING, alignof(PendingReq))));
            if (!pending_mem.get()) return false;
            pending = reinterpret_cast<PendingReq*>(pending_mem.get());
            for (io::u32 i = 0; i < MAX_PENDING; ++i) pending[i] = {};

            peers_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(PeerState) * static_cast<io::u32>(io::MAX_PEERS), alignof(PeerState))));
            if (!peers_mem.get()) return false;
            peers = reinterpret_cast<PeerState*>(peers_mem.get());
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) peers[i] = {};

            stream.sx = stream.distance * 2u + 1u;
            stream.sz = stream.sx;
            stream.y_radius = ge::server::chunk::ComputeStreamYRadius(stream.distance);
            if (stream.y_radius < STREAM_Y_RADIUS_MIN) stream.y_radius = STREAM_Y_RADIUS_MIN;
            stream.sy = static_cast<io::u32>(stream.y_radius * 2 + 1);
            stream.count = stream.sx * stream.sy * stream.sz;
            if (stream.count == 0u) return false;

            stream_order_mem.reset(static_cast<io::u8*>(io::alloc_aligned(stream.count * sizeof(io::u32), alignof(io::u32))));
            if (!stream_order_mem.get()) return false;
            stream_order = reinterpret_cast<io::u32*>(stream_order_mem.get());
            ge::server::chunk::BuildSortedStreamOrder(stream_order, stream.count, stream.sx, stream.sz, stream.distance, stream.y_radius);

            stream_state_mem.reset(static_cast<io::u8*>(io::alloc_aligned(stream.count * static_cast<io::u32>(io::MAX_PEERS), alignof(io::u8))));
            if (!stream_state_mem.get()) return false;
            stream_state = stream_state_mem.get();
            for (io::u32 i = 0; i < stream.count * static_cast<io::u32>(io::MAX_PEERS); ++i)
                stream_state[i] = StreamUnsent;

            stream_reqid_mem.reset(static_cast<io::u8*>(io::alloc_aligned(stream.count * static_cast<io::u32>(io::MAX_PEERS) * sizeof(io::u32), alignof(io::u32))));
            if (!stream_reqid_mem.get()) return false;
            stream_reqid = reinterpret_cast<io::u32*>(stream_reqid_mem.get());
            for (io::u32 i = 0; i < stream.count * static_cast<io::u32>(io::MAX_PEERS); ++i)
                stream_reqid[i] = 0u;

            stream_sent_ms_mem.reset(static_cast<io::u8*>(io::alloc_aligned(stream.count * static_cast<io::u32>(io::MAX_PEERS) * sizeof(io::u32), alignof(io::u32))));
            if (!stream_sent_ms_mem.get()) return false;
            stream_sent_ms = reinterpret_cast<io::u32*>(stream_sent_ms_mem.get());
            for (io::u32 i = 0; i < stream.count * static_cast<io::u32>(io::MAX_PEERS); ++i)
                stream_sent_ms[i] = 0u;

            sand_touched_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(ge::voxel::ChunkCoord) * SAND_SAVE_CAP, alignof(ge::voxel::ChunkCoord))));
            if (!sand_touched_mem.get()) return false;
            sand_touched = reinterpret_cast<ge::voxel::ChunkCoord*>(sand_touched_mem.get());
            for (io::u32 i = 0; i < SAND_SAVE_CAP; ++i)
                sand_touched[i] = {};

            sand_reserved_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(SandReservedCell) * SAND_MOVE_BUDGET, alignof(SandReservedCell))));
            if (!sand_reserved_mem.get()) return false;
            sand_reserved = reinterpret_cast<SandReservedCell*>(sand_reserved_mem.get());
            for (io::u32 i = 0; i < SAND_MOVE_BUDGET; ++i)
                sand_reserved[i] = {};

            sand_active_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(SandActiveCell) * SAND_ACTIVE_CAP, alignof(SandActiveCell))));
            if (!sand_active_mem.get()) return false;
            sand_active = reinterpret_cast<SandActiveCell*>(sand_active_mem.get());
            for (io::u32 i = 0; i < SAND_ACTIVE_CAP; ++i)
                sand_active[i] = {};

            water_touched_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(ge::voxel::ChunkCoord) * WATER_SAVE_CAP, alignof(ge::voxel::ChunkCoord))));
            if (!water_touched_mem.get()) return false;
            water_touched = reinterpret_cast<ge::voxel::ChunkCoord*>(water_touched_mem.get());
            for (io::u32 i = 0; i < WATER_SAVE_CAP; ++i)
                water_touched[i] = {};

            water_edits_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(WaterPendingEdit) * WATER_EDIT_BUDGET, alignof(WaterPendingEdit))));
            if (!water_edits_mem.get()) return false;
            water_edits = reinterpret_cast<WaterPendingEdit*>(water_edits_mem.get());
            for (io::u32 i = 0; i < WATER_EDIT_BUDGET; ++i)
                water_edits[i] = {};

            water_active_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(WaterActiveCell) * WATER_ACTIVE_CAP, alignof(WaterActiveCell))));
            if (!water_active_mem.get()) return false;
            water_active = reinterpret_cast<WaterActiveCell*>(water_active_mem.get());
            for (io::u32 i = 0; i < WATER_ACTIVE_CAP; ++i)
                water_active[i] = {};

            deferred_block_edits_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(DeferredBlockEdit) * BLOCK_EDIT_DEFER_CAP, alignof(DeferredBlockEdit))));
            if (!deferred_block_edits_mem.get()) return false;
            deferred_block_edits = reinterpret_cast<DeferredBlockEdit*>(deferred_block_edits_mem.get());
            for (io::u32 i = 0; i < BLOCK_EDIT_DEFER_CAP; ++i)
                deferred_block_edits[i] = {};

            io::u32 wanted_hot_sim_cap = hot_cache_target_chunks();
            if (wanted_hot_sim_cap == 0u) wanted_hot_sim_cap = 8u;
            if (wanted_hot_sim_cap < 8u) wanted_hot_sim_cap = 8u;
            if (wanted_hot_sim_cap > HOT_SIM_MAX_ENTRIES) wanted_hot_sim_cap = HOT_SIM_MAX_ENTRIES;
            hot_sim_cap = wanted_hot_sim_cap;
            hot_sim_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(HotChunkSim) * hot_sim_cap, alignof(HotChunkSim))));
            if (!hot_sim_mem.get()) {
                hot_sim = nullptr;
                hot_sim_cap = 0u;
            } else {
                hot_sim = reinterpret_cast<HotChunkSim*>(hot_sim_mem.get());
                for (io::u32 i = 0; i < hot_sim_cap; ++i) {
                    HotChunkSim& hs = hot_sim[i];
                    hs.used = false;
                    hs.coord = {};
                    hs.chunk_version = 0u;
                    hs.last_touch_ms = 0u;
                    for (io::u32 b = 0u; b < HOT_BLOCK_PACKED_BYTES; ++b)
                        hs.packed_flags[b] = 0u;
                    for (io::u32 b = 0u; b < ((ge::voxel::CHUNK_VOLUME + 7u) / 8u); ++b) {
                        hs.sand_queued[b] = 0u;
                        hs.water_queued[b] = 0u;
                    }
                }
            }
            hot_sim_last_idx = 0u;

            world_actor_ecs_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(ActorEcs), alignof(ActorEcs))));
            if (!world_actor_ecs_mem.get()) return false;
            world_actor_ecs = new (world_actor_ecs_mem.get()) ActorEcs{};
            world_actor_ecs->Reset();

            player_ecs_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(PlayerEcs), alignof(PlayerEcs))));
            if (!player_ecs_mem.get()) return false;
            player_ecs = new (player_ecs_mem.get()) PlayerEcs{};
            player_ecs->Reset();

            pool_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(io::ThreadPool), alignof(io::ThreadPool))));
            if (!pool_mem.get()) return false;
            pool = new (pool_mem.get()) io::ThreadPool{};
            unsigned qcap = 64u;
            const unsigned wanted = static_cast<unsigned>(worker.threads) * 8u;
            while (qcap < wanted) {
                if (qcap >= (0x80000000u)) {
                    qcap = wanted;
                    break;
                }
                qcap <<= 1u;
            }
            if (!pool->init(static_cast<unsigned>(worker.threads), qcap))
                return false;

            worker.slot_count = worker.threads * 4u;
            if (worker.slot_count < 8u) worker.slot_count = 8u;
            if (worker.slot_count > MAX_PENDING) worker.slot_count = MAX_PENDING;

            worker_slots_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(WorkerSlot) * worker.slot_count, alignof(WorkerSlot))));
            if (!worker_slots_mem.get()) return false;
            worker_slots = reinterpret_cast<WorkerSlot*>(worker_slots_mem.get());
            for (io::u32 i = 0; i < worker.slot_count; ++i) new (&worker_slots[i]) WorkerSlot{};

            worker_args_mem.reset(static_cast<io::u8*>(io::alloc_aligned(sizeof(WorkerArg) * worker.slot_count, alignof(WorkerArg))));
            if (!worker_args_mem.get()) return false;
            worker_args = reinterpret_cast<WorkerArg*>(worker_args_mem.get());
            for (io::u32 i = 0; i < worker.slot_count; ++i) worker_args[i] = { this, i };

            pending_head = 0u;
            pending_tail = 0u;
            pending_count = 0u;
            stats.jobs_submitted = stats.jobs_completed = stats.jobs_inflight = stats.jobs_canceled = 0u;
            stats.send_ok = stats.send_fail = stats.dropped = 0u;
            stats.send_backpressure = 0u;
            stats.send_backpressure_ticks = 0u;
            stats.send_backpressure_packets = 0u;
            stats.pos_packets = stats.pos_accept = stats.pos_reject = 0u;
            stats.pos_sync_ok = stats.pos_sync_fail = 0u;
            stats.chunk_ack_ok = stats.chunk_ack_bad = 0u;
            stats.chunk_full_sent = stats.chunk_empty_sent = 0u;
            stats.block_edits_ok = stats.block_edits_reject = 0u;
            stats.chat_rx = stats.chat_cmd = stats.chat_tx = 0u;
            stats.inventory_rx = stats.inventory_tx = 0u;
            stats.net_drop_total = 0u;
            stats.net_drop_too_small = 0u;
            stats.net_drop_bad_magic = 0u;
            stats.net_drop_bad_ver = 0u;
            stats.net_drop_bad_len = 0u;
            stats.net_drop_bad_hs = 0u;
            stats.net_drop_bad_ctrl = 0u;
            stats.net_drop_bad_mtu = 0u;
            stats.net_drop_full_peer_table = 0u;
            stats.sand_next_step_ms = 0u;
            stats.sand_chunk_cursor = 0u;
            stats.sand_linear_cursor = 0u;
            stats.sand_active_head = 0u;
            stats.sand_active_tail = 0u;
            stats.sand_active_count = 0u;
            stats.water_next_step_ms = 0u;
            stats.water_active_head = 0u;
            stats.water_active_tail = 0u;
            stats.water_active_count = 0u;
            stats.water_seed_chunk_cursor = 0u;
            stats.water_seed_linear_cursor = 0u;
            stats.water_seed_chunk_cursor = 0u;
            stats.water_seed_linear_cursor = 0u;
            deferred_block_edit_head = 0u;
            deferred_block_edit_tail = 0u;
            deferred_block_edit_count = 0u;
            worker.next_free_scan = worker.next_ready_scan = 0u;
            stats.next_stats_ms = 0u;
            stats.idle_backoff_ms = 0u;
            stats.sim_next_ms = 0u;
            stats.tps_window_start_ms = 0u;
            stats.tps_tick_count = 0u;
            region_init(io::monotonic_ms());
            return true;
        }

