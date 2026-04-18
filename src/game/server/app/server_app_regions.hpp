        // Authoritative region system: state cache, persistence, diffusion and sync.
        static constexpr io::u32 REGION_STATE_CAP = 8192u;
        static constexpr io::u32 REGION_SYNC_INTERVAL_MS = 550u;
        static constexpr io::u32 REGION_DIFFUSE_INTERVAL_MS = 900u;
        static constexpr io::u32 REGION_SAVE_INTERVAL_MS = 5000u;
        static constexpr io::u32 REGION_DIFFUSE_BUDGET = 256u;
        static constexpr io::u64 REGION_ACTIVE_TTL_MS = 30000u;

        static constexpr io::i32 REGION_MANA_DIFF_THRESHOLD = 420;
        static constexpr io::i32 REGION_DECAY_DIFF_THRESHOLD = 1200;
        static constexpr io::i32 REGION_MANA_DIFF_DIV = 20;
        static constexpr io::i32 REGION_DECAY_DIFF_DIV = 72;

        struct RegionDiskHeader {
            io::u32 magic = 0x4E474552u; // "REGN"
            io::u32 version = 1u;
            io::u32 count = 0u;
            io::u32 _pad = 0u;
        };

        struct RegionDiskEntry {
            io::u32 id_hi = 0u;
            io::u32 id_lo = 0u;
            io::u16 mana = 0u;
            io::u16 instability = 0u;
            io::u16 decay = 0u;
            io::u8 type_hint = 0u;
            io::u8 _pad = 0u;
        };

        struct RegionStateSlot {
            bool used = false;
            ge::region::Coord coord{};
            ge::region::RegionId id = 0ull;
            io::u16 mana = 0u;
            io::u16 instability = 0u;
            io::u16 decay = 0u;
            io::u8 type_hint = 0u;
            io::u8 mana_band = 0u;
            io::u8 instability_band = 0u;
            io::u8 decay_band = 0u;
            io::u64 neighbors[ge::region::NEIGHBOR_COUNT]{};
            io::u64 last_update_ms = 0u;
            io::u64 last_active_ms = 0u;
            bool dirty_runtime = false;
            bool dirty_disk = false;
        };

        struct RegionRuntimeStats {
            io::u32 loaded = 0u;
            io::u32 active = 0u;
            io::u32 dirty = 0u;
            io::u32 diffused = 0u;
            io::u32 diff_skipped = 0u;
            io::u32 sync_ok = 0u;
            io::u32 sync_fail = 0u;
            io::u32 save_ok = 0u;
            io::u32 save_fail = 0u;
        } region_stats{};

        RegionStateSlot region_slots[REGION_STATE_CAP]{};
        io::u32 region_slot_count = 0u;
        io::u32 region_diffuse_cursor = 0u;
        io::u64 region_next_diffuse_ms = 0u;
        io::u64 region_next_sync_ms = 0u;
        io::u64 region_next_save_ms = 0u;
        bool region_save_needed = false;

        IO_NODISCARD static inline io::u32 region_slot_hash(ge::region::RegionId id) noexcept {
            return ge::region::hash_region_id(id) & (REGION_STATE_CAP - 1u);
        }

        inline void region_clear_all() noexcept {
            for (io::u32 i = 0u; i < REGION_STATE_CAP; ++i)
                region_slots[i] = {};
            region_slot_count = 0u;
            region_diffuse_cursor = 0u;
            region_next_diffuse_ms = 0u;
            region_next_sync_ms = 0u;
            region_next_save_ms = 0u;
            region_save_needed = false;
            region_stats = {};
        }

        IO_NODISCARD inline io::i32 region_find_slot(ge::region::RegionId id) const noexcept {
            io::u32 at = region_slot_hash(id);
            for (io::u32 n = 0u; n < REGION_STATE_CAP; ++n) {
                const RegionStateSlot& s = region_slots[at];
                if (!s.used) return -1;
                if (s.id == id) return static_cast<io::i32>(at);
                at = (at + 1u) & (REGION_STATE_CAP - 1u);
            }
            return -1;
        }

        IO_NODISCARD inline io::u32 region_pick_evict_slot() const noexcept {
            io::u64 best_ms = static_cast<io::u64>(-1);
            io::u32 best_idx = 0u;
            for (io::u32 i = 0u; i < REGION_STATE_CAP; ++i) {
                const RegionStateSlot& s = region_slots[i];
                if (!s.used) return i;
                if (s.last_active_ms < best_ms) {
                    best_ms = s.last_active_ms;
                    best_idx = i;
                }
            }
            return best_idx;
        }

        static inline void region_refresh_bands(RegionStateSlot& slot) noexcept {
            slot.mana_band = static_cast<io::u8>(ge::region::mana_band(slot.mana));
            slot.instability_band = static_cast<io::u8>(ge::region::instability_band(slot.instability));
            slot.decay_band = static_cast<io::u8>(ge::region::decay_band(slot.decay));
        }

        static inline void region_fill_neighbors(RegionStateSlot& slot) noexcept {
            for (io::u32 i = 0u; i < ge::region::NEIGHBOR_COUNT; ++i) {
                const ge::region::Coord nc = ge::region::neighbor_coord(slot.coord, i);
                slot.neighbors[i] = ge::region::pack_region_id(nc);
            }
        }

        inline void region_init_slot(RegionStateSlot& slot, const ge::region::Coord& coord, io::u64 now_ms) noexcept {
            slot = {};
            slot.used = true;
            slot.coord = coord;
            slot.id = ge::region::pack_region_id(coord);
            ge::region::default_state_values(slot.id, slot.mana, slot.instability, slot.decay, slot.type_hint);
            region_refresh_bands(slot);
            region_fill_neighbors(slot);
            slot.last_update_ms = now_ms;
            slot.last_active_ms = now_ms;
        }

        IO_NODISCARD inline RegionStateSlot* region_ensure_by_coord(const ge::region::Coord& coord, io::u64 now_ms) noexcept {
            const ge::region::RegionId id = ge::region::pack_region_id(coord);
            io::u32 at = region_slot_hash(id);
            for (io::u32 n = 0u; n < REGION_STATE_CAP; ++n) {
                RegionStateSlot& s = region_slots[at];
                if (!s.used) {
                    region_init_slot(s, coord, now_ms);
                    ++region_slot_count;
                    return &s;
                }
                if (s.id == id) {
                    s.last_active_ms = now_ms;
                    return &s;
                }
                at = (at + 1u) & (REGION_STATE_CAP - 1u);
            }

            // Bounded memory: reuse the stalest slot.
            const io::u32 evict = region_pick_evict_slot();
            RegionStateSlot& s = region_slots[evict];
            if (s.used && s.dirty_disk)
                region_save_needed = true;
            region_init_slot(s, coord, now_ms);
            return &s;
        }

        IO_NODISCARD inline RegionStateSlot* region_ensure_by_id(ge::region::RegionId id, io::u64 now_ms) noexcept {
            const ge::region::Coord coord = ge::region::unpack_region_id(id);
            return region_ensure_by_coord(coord, now_ms);
        }

        IO_NODISCARD inline RegionStateSlot* region_ensure_by_world(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
            return region_ensure_by_coord(ge::region::coord_from_world(wx, wy, wz), now_ms);
        }

        static inline void region_mark_dirty(RegionStateSlot& s, io::u64 now_ms) noexcept {
            s.dirty_runtime = true;
            s.dirty_disk = true;
            s.last_update_ms = now_ms;
            s.last_active_ms = now_ms;
            region_refresh_bands(s);
        }

        inline void region_apply_delta(RegionStateSlot& s, io::i32 mana_delta, io::i32 instability_delta, io::i32 decay_delta, io::u64 now_ms) noexcept {
            const io::u16 old_mana = s.mana;
            const io::u16 old_instability = s.instability;
            const io::u16 old_decay = s.decay;
            s.mana = ge::region::clamp_u16_delta(s.mana, mana_delta);
            s.instability = ge::region::clamp_u16_delta(s.instability, instability_delta);
            s.decay = ge::region::clamp_u16_delta(s.decay, decay_delta);
            if (s.mana != old_mana || s.instability != old_instability || s.decay != old_decay)
                region_mark_dirty(s, now_ms);
        }

        inline void region_apply_delta_at_world(io::i32 wx, io::i32 wy, io::i32 wz,
                                                io::i32 mana_delta,
                                                io::i32 instability_delta,
                                                io::i32 decay_delta,
                                                io::u64 now_ms) noexcept {
            RegionStateSlot* s = region_ensure_by_world(wx, wy, wz, now_ms);
            if (!s) return;
            region_apply_delta(*s, mana_delta, instability_delta, decay_delta, now_ms);
        }

        // Region hook used by ward/spell gameplay events.
        // For now we keep the model intentionally simple:
        // - breaking world with magic drains more mana and raises instability/decay more
        // - placing with magic still perturbs the region, but less
        inline void region_note_ward_interaction(io::i32 wx, io::i32 wy, io::i32 wz,
                                                 bool broke_block,
                                                 io::u64 now_ms) noexcept {
            const io::i32 mana_delta = broke_block ? -180 : -80;
            const io::i32 instability_delta = broke_block ? 120 : 45;
            const io::i32 decay_delta = broke_block ? 24 : 6;
            region_apply_delta_at_world(wx, wy, wz, mana_delta, instability_delta, decay_delta, now_ms);
            region_save_needed = true;
        }

        // Calm/memory actions (for future grave/ritual systems) can use this bounded helper.
        inline void region_note_memory_repair(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
            region_apply_delta_at_world(wx, wy, wz, +320, -64, -420, now_ms);
            region_save_needed = true;
        }

        IO_NODISCARD static inline io::i32 region_border_distance_blocks(io::i32 wx, io::i32 wz) noexcept {
            return static_cast<io::i32>(ge::region::border_distance_from_world_xz(wx, wz) + 0.5f);
        }

        IO_NODISCARD static inline bool ensure_region_storage_path(io::StackOut<384>& out_path) noexcept {
            io::StackOut<260> root{};
            if (!ge::voxel::resolve_resources_root_stack(root)) return false;
            io::StackOut<320> world_dir{};
            fs::path_join(root.view(), "world", world_dir);
            if (world_dir.view().empty()) return false;
            if (!fs::is_directory(world_dir.view()) && !fs::create_directory(world_dir.view()))
                return false;
            out_path.reset();
            fs::path_join(world_dir.view(), "regions.bin", out_path);
            return !out_path.view().empty();
        }

        IO_NODISCARD inline bool region_save_to_disk() noexcept {
            io::StackOut<384> path{};
            if (!ensure_region_storage_path(path))
                return false;

            io::u32 save_count = 0u;
            for (io::u32 i = 0u; i < REGION_STATE_CAP; ++i) {
                const RegionStateSlot& s = region_slots[i];
                if (!s.used) continue;
                io::u16 dm = 0u, di = 0u, dd = 0u;
                io::u8 dt = 0u;
                ge::region::default_state_values(s.id, dm, di, dd, dt);
                if (s.dirty_disk || s.mana != dm || s.instability != di || s.decay != dd)
                    ++save_count;
            }

            fs::File f{ path.view(), io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Truncate | io::OpenMode::Binary };
            if (!f.is_open()) return false;
            RegionDiskHeader h{};
            h.count = save_count;
            const io::char_view hbytes{ reinterpret_cast<const char*>(&h), sizeof(h) };
            if (f.write(hbytes) != hbytes.size())
                return false;

            for (io::u32 i = 0u; i < REGION_STATE_CAP; ++i) {
                RegionStateSlot& s = region_slots[i];
                if (!s.used) continue;
                io::u16 dm = 0u, di = 0u, dd = 0u;
                io::u8 dt = 0u;
                ge::region::default_state_values(s.id, dm, di, dd, dt);
                if (!s.dirty_disk && s.mana == dm && s.instability == di && s.decay == dd)
                    continue;
                RegionDiskEntry e{};
                e.id_hi = ge::net::region_id_hi(s.id);
                e.id_lo = ge::net::region_id_lo(s.id);
                e.mana = s.mana;
                e.instability = s.instability;
                e.decay = s.decay;
                e.type_hint = s.type_hint;
                const io::char_view ebytes{ reinterpret_cast<const char*>(&e), sizeof(e) };
                if (f.write(ebytes) != ebytes.size())
                    return false;
                s.dirty_disk = false;
            }
            if (!f.flush()) return false;
            region_save_needed = false;
            ++region_stats.save_ok;
            return true;
        }

        inline void region_load_from_disk(io::u64 now_ms) noexcept {
            io::StackOut<384> path{};
            if (!ensure_region_storage_path(path))
                return;
            if (!fs::exists(path.view()))
                return;

            fs::File f{ path.view(), io::OpenMode::Read | io::OpenMode::Binary };
            if (!f.is_open()) return;
            RegionDiskHeader h{};
            if (!f.read_exact(&h, sizeof(h)))
                return;
            if (h.magic != 0x4E474552u || h.version != 1u)
                return;
            io::u32 loaded = 0u;
            for (io::u32 i = 0u; i < h.count; ++i) {
                RegionDiskEntry e{};
                if (!f.read_exact(&e, sizeof(e)))
                    break;
                const ge::region::RegionId id = ge::net::compose_region_id(e.id_hi, e.id_lo);
                RegionStateSlot* slot = region_ensure_by_id(id, now_ms);
                if (!slot) continue;
                slot->mana = e.mana;
                slot->instability = e.instability;
                slot->decay = e.decay;
                slot->type_hint = e.type_hint;
                slot->dirty_runtime = false;
                slot->dirty_disk = false;
                slot->last_update_ms = now_ms;
                slot->last_active_ms = now_ms;
                region_refresh_bands(*slot);
                ++loaded;
            }
            region_stats.loaded = loaded;
        }

        inline void region_init(io::u64 now_ms) noexcept {
            region_clear_all();
            region_load_from_disk(now_ms);
            region_next_diffuse_ms = now_ms + REGION_DIFFUSE_INTERVAL_MS;
            region_next_sync_ms = now_ms + REGION_SYNC_INTERVAL_MS;
            region_next_save_ms = now_ms + REGION_SAVE_INTERVAL_MS;
        }

        inline void region_shutdown(io::u64 now_ms) noexcept {
            (void)now_ms;
            if (region_save_needed)
                (void)region_save_to_disk();
        }

        inline void region_mark_active_for_peer(const PeerState& p, io::u64 now_ms) noexcept {
            if (!p.used || !p.has_auth) return;
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(floor_to_i32(p.auth_x), floor_to_i32(p.auth_y), floor_to_i32(p.auth_z), cc, lx, ly, lz);
            io::i32 radius = static_cast<io::i32>(stream.hot_chunks) + 1;
            if (radius < 1) radius = 1;
            if (radius > 4) radius = 4;
            const io::i32 wy = floor_to_i32(p.auth_y);
            for (io::i32 dz = -radius; dz <= radius; ++dz) {
                for (io::i32 dx = -radius; dx <= radius; ++dx) {
                    const io::i32 wx = (cc.x + dx) * static_cast<io::i32>(ge::voxel::CHUNK_W) + static_cast<io::i32>(ge::voxel::CHUNK_W / 2);
                    const io::i32 wz = (cc.z + dz) * static_cast<io::i32>(ge::voxel::CHUNK_D) + static_cast<io::i32>(ge::voxel::CHUNK_D / 2);
                    RegionStateSlot* s = region_ensure_by_world(wx, wy, wz, now_ms);
                    if (s) s->last_active_ms = now_ms;
                }
            }
        }

        inline void region_update_active_set(io::u64 now_ms) noexcept {
            if (!peers) return;
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                if (!peers[i].used || !peers[i].has_auth) continue;
                region_mark_active_for_peer(peers[i], now_ms);
            }
        }

        inline void region_diffusion_step(io::u64 now_ms, bool force = false) noexcept {
            if (!force) {
                if (region_next_diffuse_ms != 0u && now_ms < region_next_diffuse_ms) return;
                region_next_diffuse_ms = now_ms + REGION_DIFFUSE_INTERVAL_MS;
            }
            if (region_slot_count == 0u) return;

            io::u32 processed = 0u;
            io::u32 touched = 0u;
            for (io::u32 n = 0u; n < REGION_STATE_CAP && processed < REGION_DIFFUSE_BUDGET; ++n) {
                const io::u32 idx = (region_diffuse_cursor + n) & (REGION_STATE_CAP - 1u);
                RegionStateSlot& src = region_slots[idx];
                if (!src.used) continue;
                if (now_ms - src.last_active_ms > REGION_ACTIVE_TTL_MS && !src.dirty_runtime) {
                    ++region_stats.diff_skipped;
                    continue;
                }
                ++processed;
                for (io::u32 k = 0u; k < ge::region::NEIGHBOR_COUNT; ++k) {
                    RegionStateSlot* dst = region_ensure_by_id(src.neighbors[k], now_ms);
                    if (!dst || !dst->used || dst->id == src.id) continue;

                    const io::i32 mana_delta = static_cast<io::i32>(src.mana) - static_cast<io::i32>(dst->mana);
                    const io::i32 decay_delta = static_cast<io::i32>(src.decay) - static_cast<io::i32>(dst->decay);

                    if (ge::region::abs_i32(mana_delta) > REGION_MANA_DIFF_THRESHOLD) {
                        io::i32 transfer = mana_delta / REGION_MANA_DIFF_DIV;
                        if (transfer == 0) transfer = (mana_delta > 0) ? 1 : -1;
                        src.mana = ge::region::clamp_u16_delta(src.mana, -transfer);
                        dst->mana = ge::region::clamp_u16_delta(dst->mana, transfer);
                        region_mark_dirty(src, now_ms);
                        region_mark_dirty(*dst, now_ms);
                        ++touched;
                    }

                    if (ge::region::abs_i32(decay_delta) > REGION_DECAY_DIFF_THRESHOLD) {
                        io::i32 transfer = decay_delta / REGION_DECAY_DIFF_DIV;
                        if (transfer == 0) transfer = (decay_delta > 0) ? 1 : -1;
                        src.decay = ge::region::clamp_u16_delta(src.decay, -transfer);
                        dst->decay = ge::region::clamp_u16_delta(dst->decay, transfer);
                        region_mark_dirty(src, now_ms);
                        region_mark_dirty(*dst, now_ms);
                        ++touched;
                    }
                }
                src.dirty_runtime = false;
            }

            region_diffuse_cursor = (region_diffuse_cursor + processed) & (REGION_STATE_CAP - 1u);
            region_stats.diffused += touched;
            if (touched > 0u) region_save_needed = true;
        }

        IO_NODISCARD inline bool region_send_to_peer(io::u16 peer_index, io::u64 now_ms, io::UdpChan chan) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            const PeerState& p = peers[peer_index];
            if (!p.used || !p.has_auth) return false;

            const io::i32 wx = floor_to_i32(p.auth_x);
            const io::i32 wy = floor_to_i32(p.auth_y);
            const io::i32 wz = floor_to_i32(p.auth_z);
            ge::region::Coord center = ge::region::coord_from_world(wx, wy, wz);

            ge::net::RegionStateSample sample{};
            RegionStateSlot* center_slot = region_ensure_by_coord(center, now_ms);
            if (!center_slot) return false;

            auto push_entry = [&](RegionStateSlot* slot, io::u8 flags) noexcept {
                if (!slot) return;
                for (io::u32 i = 0u; i < sample.count; ++i) {
                    if (sample.entries[i].region_id == slot->id) return;
                }
                if (sample.count >= ge::net::REGION_SYNC_MAX_ENTRIES) return;
                ge::net::RegionEntrySample& out = sample.entries[sample.count++];
                out.region_id = slot->id;
                out.mana = slot->mana;
                out.instability = slot->instability;
                out.decay = slot->decay;
                out.bands = ge::region::packed_bands(
                    static_cast<ge::region::ManaBand>(slot->mana_band),
                    static_cast<ge::region::InstabilityBand>(slot->instability_band),
                    static_cast<ge::region::DecayBand>(slot->decay_band));
                out.flags = flags;
            };

            push_entry(center_slot, ge::net::REGION_ENTRY_FLAG_CURRENT);
            for (io::u32 i = 0u; i < ge::region::NEIGHBOR_COUNT; ++i)
                push_entry(region_ensure_by_id(center_slot->neighbors[i], now_ms), ge::net::REGION_ENTRY_FLAG_NEIGHBOR);
            ge::region::Coord up = center;
            ge::region::Coord down = center;
            ++up.band_y;
            --down.band_y;
            push_entry(region_ensure_by_coord(up, now_ms),
                       static_cast<io::u8>(ge::net::REGION_ENTRY_FLAG_NEIGHBOR | ge::net::REGION_ENTRY_FLAG_VERTICAL));
            push_entry(region_ensure_by_coord(down, now_ms),
                       static_cast<io::u8>(ge::net::REGION_ENTRY_FLAG_NEIGHBOR | ge::net::REGION_ENTRY_FLAG_VERTICAL));

            ge::net::S2C_RegionState wire{};
            ge::net::encode_s2c_region_state(sample, wire);
            const bool ok = loop.send_to_peer(
                p.ep,
                ge::net::PK_S2C_REGION_STATE,
                chan,
                io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                now_ms);
            if (ok) ++region_stats.sync_ok;
            else ++region_stats.sync_fail;
            return ok;
        }

        inline void region_sync_peers(io::u64 now_ms, io::UdpChan chan = io::UdpChan::Reliable) noexcept {
            if (region_next_sync_ms != 0u && now_ms < region_next_sync_ms) return;
            region_next_sync_ms = now_ms + REGION_SYNC_INTERVAL_MS;
            if (!peers) return;
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                if (!peers[i].used || !peers[i].has_auth) continue;
                (void)region_send_to_peer(i, now_ms, chan);
            }
        }

        inline void region_save_periodic(io::u64 now_ms) noexcept {
            if (!region_save_needed) return;
            if (region_next_save_ms != 0u && now_ms < region_next_save_ms) return;
            region_next_save_ms = now_ms + REGION_SAVE_INTERVAL_MS;
            if (!region_save_to_disk())
                ++region_stats.save_fail;
        }

        inline void region_count_snapshot(io::u64 now_ms, io::u32& out_active, io::u32& out_dirty) const noexcept {
            out_active = 0u;
            out_dirty = 0u;
            for (io::u32 i = 0u; i < REGION_STATE_CAP; ++i) {
                const RegionStateSlot& s = region_slots[i];
                if (!s.used) continue;
                if (now_ms - s.last_active_ms <= REGION_ACTIVE_TTL_MS) ++out_active;
                if (s.dirty_disk || s.dirty_runtime) ++out_dirty;
            }
        }

        IO_NODISCARD inline bool region_debug_get_for_peer(io::u16 peer_index, RegionStateSlot& out) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            const PeerState& p = peers[peer_index];
            if (!p.used || !p.has_auth) return false;
            RegionStateSlot* slot = region_ensure_by_world(floor_to_i32(p.auth_x), floor_to_i32(p.auth_y), floor_to_i32(p.auth_z), io::monotonic_ms());
            if (!slot) return false;
            out = *slot;
            return true;
        }
