        IO_NODISCARD inline ge::item::Stack* peer_inventory_slot(PeerState& p,
                                                                 ge::item::SlotRegion region,
                                                                 io::u8 index) noexcept {
            return ge::item::slot_ptr(p.inventory, region, index);
        }

        IO_NODISCARD inline const ge::item::Stack* peer_inventory_slot(const PeerState& p,
                                                                       ge::item::SlotRegion region,
                                                                       io::u8 index) const noexcept {
            return ge::item::slot_ptr(p.inventory, region, index);
        }

        IO_NODISCARD static inline io::u32 ward_hash32(io::u32 x) noexcept {
            x ^= x >> 16u;
            x *= 0x7FEB352Du;
            x ^= x >> 15u;
            x *= 0x846CA68Bu;
            x ^= x >> 16u;
            return x;
        }

        IO_NODISCARD inline bool is_spell_item_id(ge::item::Id id) const noexcept {
            return ge::item::def(id).category == ge::item::Category::Spells;
        }

        IO_NODISCARD inline WardInstance* find_ward_instance(PeerState& p, io::u16 token) noexcept {
            if (token == 0u) return nullptr;
            for (io::u32 i = 0u; i < WARD_INSTANCE_CAP; ++i) {
                WardInstance& inst = p.ward_instances[i];
                if (!inst.active) continue;
                if (inst.token == token) return &inst;
            }
            return nullptr;
        }

        IO_NODISCARD inline const WardInstance* find_ward_instance(const PeerState& p, io::u16 token) const noexcept {
            return find_ward_instance(const_cast<PeerState&>(p), token);
        }

        inline void roll_ward_instance(WardInstance& inst, io::u16 token) noexcept {
            const io::u32 h0 = ward_hash32(static_cast<io::u32>(token) * 17u + 13u);
            const io::u32 h1 = ward_hash32(static_cast<io::u32>(token) * 31u + 7u);
            const io::u32 h2 = ward_hash32(static_cast<io::u32>(token) * 47u + 19u);
            const io::u32 family = h0 % 3u; // 0 stable, 1 fast, 2 brutal

            inst.active = true;
            inst.token = token;
            for (io::u32 i = 0u; i < WARD_SLOT_COUNT; ++i)
                inst.spells[i] = {};

            if (family == 0u) {
                inst.slots_available = static_cast<io::u8>(18u + (h1 % 10u)); // 18..27
                inst.stat_speed_x100 = static_cast<io::u16>(120u + (h1 % 120u)); // 1.20..2.39
                inst.stat_delay_cast_x1000 = static_cast<io::u16>(650u + (h2 % 700u)); // 0.65..1.349
                inst.stat_delay_reload_x1000 = static_cast<io::u16>(1650u + (h0 % 1500u)); // 1.65..3.149
                inst.stat_spread_x100 = static_cast<io::u16>(220u + (h2 % 560u)); // 2.20..7.79
            } else if (family == 1u) {
                inst.slots_available = static_cast<io::u8>(8u + (h2 % 7u)); // 8..14
                inst.stat_speed_x100 = static_cast<io::u16>(180u + (h0 % 220u)); // 1.80..3.99
                inst.stat_delay_cast_x1000 = static_cast<io::u16>(220u + (h1 % 420u)); // 0.22..0.639
                inst.stat_delay_reload_x1000 = static_cast<io::u16>(950u + (h2 % 950u)); // 0.95..1.899
                inst.stat_spread_x100 = static_cast<io::u16>(720u + (h1 % 1420u)); // 7.20..21.39
            } else {
                inst.slots_available = static_cast<io::u8>(6u + (h1 % 7u)); // 6..12
                inst.stat_speed_x100 = static_cast<io::u16>(260u + (h2 % 260u)); // 2.60..5.19
                inst.stat_delay_cast_x1000 = static_cast<io::u16>(420u + (h0 % 620u)); // 0.42..1.039
                inst.stat_delay_reload_x1000 = static_cast<io::u16>(1900u + (h1 % 1900u)); // 1.90..3.799
                inst.stat_spread_x100 = static_cast<io::u16>(1200u + (h2 % 2000u)); // 12.00..31.99
            }

            if (inst.slots_available == 0u) inst.slots_available = 1u;
            if (inst.slots_available > WARD_SLOT_COUNT)
                inst.slots_available = static_cast<io::u8>(WARD_SLOT_COUNT);
        }

        IO_NODISCARD inline bool inventory_has_ward_token(const PeerState& p, io::u16 token) const noexcept {
            if (token == 0u) return false;
            for (io::u32 i = 0u; i < ge::item::HOTBAR_SLOT_COUNT; ++i) {
                const ge::item::Stack& s = p.inventory.hotbar[i];
                if (s.id == ge::item::Id::SpellWard && s.count > 0u && s.freshness == token)
                    return true;
            }
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) {
                const ge::item::Stack& s0 = p.inventory.spelling_wards[i];
                if (s0.id == ge::item::Id::SpellWard && s0.count > 0u && s0.freshness == token)
                    return true;
            }
            const ge::item::Stack& cursor = p.inventory.cursor;
            if (cursor.id == ge::item::Id::SpellWard && cursor.count > 0u && cursor.freshness == token)
                return true;
            const ge::item::Stack& trash = p.inventory.trash;
            if (trash.id == ge::item::Id::SpellWard && trash.count > 0u && trash.freshness == token)
                return true;
            return false;
        }

        IO_NODISCARD inline io::u16 alloc_ward_token(PeerState& p) noexcept {
            for (io::u32 n = 0u; n < 0xFFFFu; ++n) {
                io::u16 token = p.ward_next_token++;
                if (p.ward_next_token == 0u)
                    p.ward_next_token = 1u;
                if (token == 0u || token == ge::item::FRESHNESS_MAX)
                    continue;
                if (!find_ward_instance(p, token))
                    return token;
            }
            return 0u;
        }

        IO_NODISCARD inline WardInstance* ensure_ward_instance(PeerState& p, io::u16 token) noexcept {
            if (token == 0u) return nullptr;
            WardInstance* existing = find_ward_instance(p, token);
            if (existing) return existing;
            for (io::u32 i = 0u; i < WARD_INSTANCE_CAP; ++i) {
                WardInstance& inst = p.ward_instances[i];
                if (inst.active) continue;
                roll_ward_instance(inst, token);
                return &inst;
            }
            // Bounded memory fallback: recycle first slot deterministically.
            roll_ward_instance(p.ward_instances[0], token);
            return &p.ward_instances[0];
        }

        inline void ensure_inventory_ward_tokens(PeerState& p) noexcept {
            auto ensure_stack = [&](ge::item::Stack& s) noexcept {
                ge::item::normalize(s);
                if (s.id != ge::item::Id::SpellWard || s.count == 0u)
                    return;
                io::u16 token = s.freshness;
                if (token == 0u || token == ge::item::FRESHNESS_MAX)
                    token = alloc_ward_token(p);
                if (token == 0u)
                    return;
                s.freshness = token;
                (void)ensure_ward_instance(p, token);
            };

            for (io::u32 i = 0u; i < ge::item::HOTBAR_SLOT_COUNT; ++i)
                ensure_stack(p.inventory.hotbar[i]);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                ensure_stack(p.inventory.spelling_wards[i]);
            ensure_stack(p.inventory.cursor);
            ensure_stack(p.inventory.trash);

            // Keep detached instances alive for this peer session.
            // This preserves staff identity when it temporarily leaves inventory
            // (for example via drop/pickup flows) while staying within bounded memory.
        }

        inline void fill_starter_inventory(PeerState& p) noexcept {
            ge::item::reset_player_inventory(p.inventory);
            for (io::u32 i = 0u; i < WARD_INSTANCE_CAP; ++i)
                p.ward_instances[i] = {};
            p.ward_next_token = 1u;
            p.inventory.selected_hotbar = 0u;
            p.inventory.hotbar[0] = ge::item::make_stack(ge::item::Id::GrassBlock, 32u);
            p.inventory.hotbar[1] = ge::item::make_stack(ge::item::Id::StoneBlock, 24u);
            p.inventory.hotbar[2] = ge::item::make_stack(ge::item::Id::SandBlock, 24u);
            p.inventory.hotbar[3] = ge::item::make_stack(ge::item::Id::LogBlock, 16u);
            p.inventory.hotbar[4] = ge::item::make_stack(ge::item::Id::Potato, 6u);
            p.inventory.hotbar[5] = ge::item::make_stack(ge::item::Id::SpellWard, 1u);
            p.inventory.hotbar[8] = ge::item::make_stack(ge::item::Id::RustyDagger, 1u);
            p.inventory.blocks[0] = ge::item::make_stack(ge::item::Id::DirtBlock, 32u);
            p.inventory.blocks[1] = ge::item::make_stack(ge::item::Id::LeavesBlock, 24u);
            p.inventory.consumables[0] = ge::item::make_stack(ge::item::Id::Potato, 8u);
            p.inventory.spelling_wards[0] = ge::item::make_stack(ge::item::Id::SpellWard, 1u);
            p.inventory.spells[0] = ge::item::make_stack(ge::item::Id::SpellBolt, 6u);
            p.inventory.spells[1] = ge::item::make_stack(ge::item::Id::SpellDig, 4u);
            p.inventory.spells[2] = ge::item::make_stack(ge::item::Id::SpellBurst, 5u);
            p.inventory.spells[3] = ge::item::make_stack(ge::item::Id::SpellBeam, 4u);
            p.inventory.spells[4] = ge::item::make_stack(ge::item::Id::SpellOrb, 4u);
            p.inventory.spells[5] = ge::item::make_stack(ge::item::Id::SpellMine, 4u);
            p.inventory.spells[6] = ge::item::make_stack(ge::item::Id::SpellShieldPulse, 4u);
            p.inventory.spells[7] = ge::item::make_stack(ge::item::Id::SpellMark, 4u);
            p.inventory.spells[8] = ge::item::make_stack(ge::item::Id::SpellPull, 4u);
            p.inventory.spells[9] = ge::item::make_stack(ge::item::Id::SpellBlinkStep, 4u);
            ensure_inventory_ward_tokens(p);
            p.last_inventory_decay_ms = io::monotonic_ms();
            p.last_inventory_sync_ms = 0u;
            p.inventory_state_hash = 0u;
            p.last_sent_inventory_hash = 0u;
        }

        IO_NODISCARD inline bool inventory_has_item(const ge::item::PlayerInventory& inv, ge::item::Id id) const noexcept {
            if (id == ge::item::Id::None) return false;
            for (io::u32 i = 0u; i < ge::item::HOTBAR_SLOT_COUNT; ++i)
                if (inv.hotbar[i].id == id && inv.hotbar[i].count > 0u) return true;
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) {
                if (inv.blocks[i].id == id && inv.blocks[i].count > 0u) return true;
                if (inv.consumables[i].id == id && inv.consumables[i].count > 0u) return true;
                if (inv.materials[i].id == id && inv.materials[i].count > 0u) return true;
                if (inv.spelling_wards[i].id == id && inv.spelling_wards[i].count > 0u) return true;
                if (inv.spells[i].id == id && inv.spells[i].count > 0u) return true;
            }
            if (inv.cursor.id == id && inv.cursor.count > 0u) return true;
            if (inv.trash.id == id && inv.trash.count > 0u) return true;
            return false;
        }

        IO_NODISCARD inline bool ensure_respawn_dagger(PeerState& p) noexcept {
            if (inventory_has_item(p.inventory, ge::item::Id::RustyDagger))
                return false;
            ge::item::Stack dagger = ge::item::make_stack(ge::item::Id::RustyDagger, 1u);
            return ge::item::add_to_inventory(p.inventory, dagger);
        }

        IO_NODISCARD static inline io::u32 hash_inventory_stack(const ge::item::Stack& s) noexcept {
            io::u32 h = 2166136261u;
            const io::u32 v0 = static_cast<io::u32>(s.id);
            const io::u32 v1 = static_cast<io::u32>(s.count);
            const io::u32 v2 = static_cast<io::u32>(s.freshness);
            const io::u8 bytes[6]{
                static_cast<io::u8>(v0 & 0xFFu),
                static_cast<io::u8>((v0 >> 8u) & 0xFFu),
                static_cast<io::u8>(v1 & 0xFFu),
                static_cast<io::u8>((v1 >> 8u) & 0xFFu),
                static_cast<io::u8>(v2 & 0xFFu),
                static_cast<io::u8>((v2 >> 8u) & 0xFFu)
            };
            for (io::u32 i = 0u; i < 6u; ++i) {
                h ^= bytes[i];
                h *= 16777619u;
            }
            return h;
        }

        IO_NODISCARD static inline io::u32 hash_player_inventory(const ge::item::PlayerInventory& inv) noexcept {
            io::u32 h = 2166136261u;
            const auto mix_stack = [&](const ge::item::Stack& s) noexcept {
                const io::u32 hs = hash_inventory_stack(s);
                h ^= static_cast<io::u8>((hs >> 0u) & 0xFFu);  h *= 16777619u;
                h ^= static_cast<io::u8>((hs >> 8u) & 0xFFu);  h *= 16777619u;
                h ^= static_cast<io::u8>((hs >> 16u) & 0xFFu); h *= 16777619u;
                h ^= static_cast<io::u8>((hs >> 24u) & 0xFFu); h *= 16777619u;
            };

            for (io::u32 i = 0u; i < ge::item::HOTBAR_SLOT_COUNT; ++i) mix_stack(inv.hotbar[i]);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) mix_stack(inv.blocks[i]);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) mix_stack(inv.consumables[i]);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) mix_stack(inv.materials[i]);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) mix_stack(inv.spelling_wards[i]);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) mix_stack(inv.spells[i]);
            mix_stack(inv.cursor);
            mix_stack(inv.trash);
            h ^= inv.selected_hotbar;
            h *= 16777619u;
            return h;
        }

        IO_NODISCARD inline bool send_inventory_to_peer(io::u16 peer_index, io::u64 now_ms, io::UdpChan chan) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            PeerState& p = peers[peer_index];
            if (!p.used) return false;
            ensure_inventory_ward_tokens(p);
            ge::net::InventoryStateSample sample{};
            sample.inventory = p.inventory;
            sample.flags = 0u;
            ge::net::S2C_InventoryState wire{};
            ge::net::encode_s2c_inventory_state(sample, wire);
            const bool ok = loop.send_to_peer(
                p.ep,
                ge::net::PK_S2C_INVENTORY_STATE,
                chan,
                io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                now_ms);
            if (ok) {
                ++stats.send_ok;
                ++stats.inventory_tx;
                peers[peer_index].last_inventory_sync_ms = now_ms;
                peers[peer_index].last_sent_inventory_hash = hash_player_inventory(peers[peer_index].inventory);
            } else {
                note_packet_backpressure();
                ++stats.send_fail;
            }
            if (ok)
                send_all_ward_configs_to_peer(peer_index, now_ms, chan);
            return ok;
        }

        IO_NODISCARD inline bool send_ward_config_state_to_peer(io::u16 peer_index,
                                                                io::u16 token,
                                                                io::u64 now_ms,
                                                                io::UdpChan chan) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            PeerState& p = peers[peer_index];
            if (!p.used) return false;
            if (!inventory_has_ward_token(p, token)) return false;
            const WardInstance* inst = find_ward_instance(p, token);
            if (!inst || !inst->active) return false;

            ge::net::WardConfigStateSample sample{};
            sample.ward_instance = token;
            sample.slots_available = inst->slots_available;
            sample.valid = true;
            sample.stat_speed = static_cast<float>(inst->stat_speed_x100) * 0.01f;
            sample.stat_delay_cast = static_cast<float>(inst->stat_delay_cast_x1000) * 0.001f;
            sample.stat_delay_reload = static_cast<float>(inst->stat_delay_reload_x1000) * 0.001f;
            sample.stat_spread = static_cast<float>(inst->stat_spread_x100) * 0.01f;
            for (io::u32 i = 0u; i < WARD_SLOT_COUNT; ++i)
                sample.spells[i] = inst->spells[i];

            ge::net::S2C_WardConfigState wire{};
            ge::net::encode_s2c_ward_config_state(sample, wire);
            const bool ok = loop.send_to_peer(
                p.ep,
                ge::net::PK_S2C_WARD_CONFIG_STATE,
                chan,
                io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                now_ms);
            if (ok) ++stats.send_ok;
            else {
                note_packet_backpressure();
                ++stats.send_fail;
            }
            return ok;
        }

        inline void send_all_ward_configs_to_peer(io::u16 peer_index, io::u64 now_ms, io::UdpChan chan) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            PeerState& p = peers[peer_index];
            if (!p.used) return;
            ensure_inventory_ward_tokens(p);
            for (io::u32 i = 0u; i < WARD_INSTANCE_CAP; ++i) {
                const WardInstance& inst = p.ward_instances[i];
                if (!inst.active || inst.token == 0u) continue;
                if (!inventory_has_ward_token(p, inst.token)) continue;
                (void)send_ward_config_state_to_peer(peer_index, inst.token, now_ms, chan);
            }
        }

        inline void sync_inventory_if_needed(io::u16 peer_index, io::u64 now_ms, bool force = false) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            PeerState& p = peers[peer_index];
            if (!p.used) return;
            static constexpr io::u64 INVENTORY_PASSIVE_SYNC_INTERVAL_MS = 2000u;
            ensure_inventory_ward_tokens(p);
            p.inventory_state_hash = hash_player_inventory(p.inventory);
            if (!force && p.last_inventory_sync_ms != 0u &&
                now_ms - p.last_inventory_sync_ms < INVENTORY_PASSIVE_SYNC_INTERVAL_MS)
                return;
            if (!force && p.inventory_state_hash == p.last_sent_inventory_hash)
                return;
            (void)send_inventory_to_peer(peer_index, now_ms, io::UdpChan::Reliable);
        }

        inline void age_player_inventory(PeerState& p, io::u64 now_ms) noexcept {
            if (p.last_inventory_decay_ms == 0u) {
                p.last_inventory_decay_ms = now_ms;
                return;
            }
            if (now_ms <= p.last_inventory_decay_ms) return;
            const io::u64 delta_ms = now_ms - p.last_inventory_decay_ms;
            p.last_inventory_decay_ms = now_ms;
            for (io::u32 i = 0u; i < ge::item::HOTBAR_SLOT_COUNT; ++i)
                ge::item::age_stack(p.inventory.hotbar[i], delta_ms);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                ge::item::age_stack(p.inventory.blocks[i], delta_ms);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                ge::item::age_stack(p.inventory.consumables[i], delta_ms);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                ge::item::age_stack(p.inventory.materials[i], delta_ms);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                ge::item::age_stack(p.inventory.spelling_wards[i], delta_ms);
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                ge::item::age_stack(p.inventory.spells[i], delta_ms);
            ge::item::age_stack(p.inventory.cursor, delta_ms);
            ge::item::age_stack(p.inventory.trash, delta_ms);
        }

        IO_NODISCARD inline bool give_item_to_peer(io::u16 peer_index, ge::item::Stack stack, io::u64 now_ms, bool sync_now = true) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            PeerState& p = peers[peer_index];
            if (!p.used) return false;
            age_player_inventory(p, now_ms);
            if (!ge::item::add_to_inventory(p.inventory, stack))
                return false;
            ensure_inventory_ward_tokens(p);
            if (sync_now)
                sync_inventory_if_needed(peer_index, now_ms, true);
            return true;
        }

        IO_NODISCARD inline io::i32 find_peer_by_name(io::char_view target_name) const noexcept {
            if (!peers) return -1;
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used) continue;
                const io::char_view peer_name{ p.name_utf8, p.name_len };
                if (peer_name.empty()) continue;
                if (eq_icase(peer_name, target_name))
                    return static_cast<io::i32>(i);
            }
            return -1;
        }

        IO_NODISCARD inline io::u32 roster_peer_count() const noexcept {
            if (!peers) return 0u;
            io::u32 n = 0u;
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i)
                if (peers[i].used) ++n;
            return n;
        }

        IO_NODISCARD inline ge::net::PlayerRosterEntry make_roster_entry(io::u16 peer_index) const noexcept {
            ge::net::PlayerRosterEntry out{};
            out.server_index = peer_index;
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS))
                return out;

            const PeerState& p = peers[peer_index];
            io::StackOut<32> fallback_name{};
            io::char_view name = (p.name_len > 0u)
                ? io::char_view{ p.name_utf8, p.name_len }
                : peer_fallback_name(peer_index, fallback_name);
            if (name.size() > ge::net::PLAYER_NICK_BYTES)
                name = name.slice(0u, ge::net::PLAYER_NICK_BYTES);

            out.name_len = static_cast<io::u8>(name.size());
            out.signal_quality = ge::net::signal_quality_from_nibble(p.signal_quality_nibble);
            for (io::u32 i = 0u; i < ge::net::PLAYER_NICK_BYTES; ++i)
                out.name[i] = (i < out.name_len) ? name[i] : '\0';
            return out;
        }

        IO_NODISCARD inline bool send_roster_page_to_peer(io::u16 peer_index,
                                                          const ge::net::PlayerRosterPage& page,
                                                          io::u64 now_ms,
                                                          io::UdpChan chan) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            const PeerState& p = peers[peer_index];
            if (!p.used) return false;

            ge::net::S2C_PlayerRosterPage header{};
            ge::net::encode_s2c_player_roster_page_header(page, header);
            io::u8 payload[sizeof(ge::net::S2C_PlayerRosterPage) +
                          sizeof(ge::net::PlayerRosterEntryWire) * ge::net::PLAYER_ROSTER_PAGE_MAX_ENTRIES]{};
            io::usize cursor = 0u;
            for (io::usize i = 0u; i < sizeof(header); ++i)
                payload[cursor++] = reinterpret_cast<const io::u8*>(&header)[i];
            for (io::u32 i = 0u; i < page.count; ++i) {
                ge::net::PlayerRosterEntryWire wire_entry{};
                ge::net::encode_player_roster_entry(page.entries[i], wire_entry);
                for (io::usize b = 0u; b < sizeof(wire_entry); ++b)
                    payload[cursor++] = reinterpret_cast<const io::u8*>(&wire_entry)[b];
            }

            const bool ok = loop.send_to_peer(
                p.ep,
                ge::net::PK_S2C_PLAYER_ROSTER_PAGE,
                chan,
                io::byte_view{ payload, cursor },
                now_ms);
            if (ok) ++stats.send_ok;
            else {
                note_packet_backpressure();
                ++stats.send_fail;
            }
            return ok;
        }

        inline void send_roster_window_to_peer(io::u16 peer_index,
                                               io::u16 start_index,
                                               io::u16 max_entries,
                                               io::u64 now_ms,
                                               io::u8 extra_flags = 0u) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            if (!peers[peer_index].used) return;

            io::u16 total_online = static_cast<io::u16>(roster_peer_count() > 0xFFFFu ? 0xFFFFu : roster_peer_count());
            if (max_entries == 0u)
                max_entries = static_cast<io::u16>(ge::net::PLAYER_ROSTER_CLIENT_CAP);
            if (max_entries > ge::net::PLAYER_ROSTER_CLIENT_CAP)
                max_entries = static_cast<io::u16>(ge::net::PLAYER_ROSTER_CLIENT_CAP);
            if (start_index >= total_online)
                start_index = total_online;

            io::u16 remaining = max_entries;
            io::u16 current_start = start_index;
            bool first_page = true;
            while (remaining > 0u || first_page) {
                ge::net::PlayerRosterPage page{};
                page.start_index = current_start;
                page.total_online = total_online;
                page.flags = static_cast<io::u8>(extra_flags | (first_page ? ge::net::PLAYER_ROSTER_PAGE_FLAG_RESET : 0u));
                page.count = 0u;
                io::u16 skip = current_start;

                for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                    if (!peers[i].used) continue;
                    if (skip > 0u) {
                        --skip;
                        continue;
                    }
                    page.entries[page.count++] = make_roster_entry(i);
                    if (page.count >= ge::net::PLAYER_ROSTER_PAGE_MAX_ENTRIES)
                        break;
                    if (page.count >= remaining)
                        break;
                }

                (void)send_roster_page_to_peer(peer_index, page, now_ms, io::UdpChan::Reliable);
                if (page.count == 0u)
                    break;
                current_start = static_cast<io::u16>(current_start + page.count);
                if (remaining > page.count) remaining = static_cast<io::u16>(remaining - page.count);
                else remaining = 0u;
                first_page = false;
            }
        }

        inline void broadcast_roster_add(io::u16 source_peer_index, io::u64 now_ms) noexcept {
            if (!peers || source_peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            if (roster_peer_count() > ge::net::PLAYER_ROSTER_CLIENT_CAP) return;

            const ge::net::PlayerRosterEntry entry = make_roster_entry(source_peer_index);
            ge::net::S2C_PlayerRosterAdd wire{};
            ge::net::encode_s2c_player_roster_add(entry, wire);
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used) continue;
                const bool ok = loop.send_to_peer(
                    p.ep,
                    ge::net::PK_S2C_PLAYER_ROSTER_ADD,
                    io::UdpChan::Reliable,
                    io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                    now_ms);
                if (ok) ++stats.send_ok;
                else {
                    note_packet_backpressure();
                    ++stats.send_fail;
                }
            }
        }

        inline void broadcast_roster_remove(io::u16 source_peer_index, io::u64 now_ms) noexcept {
            if (!peers || source_peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            ge::net::PlayerRosterRemove sample{};
            sample.server_index = source_peer_index;
            ge::net::S2C_PlayerRosterRemove wire{};
            ge::net::encode_s2c_player_roster_remove(sample, wire);
            for (io::u16 i = 0u; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used) continue;
                if (i == source_peer_index) continue;
                const bool ok = loop.send_to_peer(
                    p.ep,
                    ge::net::PK_S2C_PLAYER_ROSTER_REMOVE,
                    io::UdpChan::Reliable,
                    io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) },
                    now_ms);
                if (ok) ++stats.send_ok;
                else {
                    note_packet_backpressure();
                    ++stats.send_fail;
                }
            }
        }

        inline void enforce_world_item_chunk_cap(const ge::voxel::ChunkCoord& chunk_coord, io::u32 keep_actor_index) noexcept {
            if (!world_actor_ecs) return;
            ActorEcs& ecs = *world_actor_ecs;
            io::u32 count = 0u;
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
                if (!ge::voxel::coord_eq(cc, chunk_coord)) continue;
                ++count;
            }
            while (count > WORLD_ITEMS_PER_CHUNK_CAP) {
                io::u32 victim = WORLD_ACTOR_CAP;
                io::u16 victim_id = 0xFFFFu;
                for (io::u16 i = 0u; i < WORLD_ACTOR_CAP; ++i) {
                    if (i == keep_actor_index) continue;
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
                    if (!ge::voxel::coord_eq(cc, chunk_coord)) continue;
                    const io::u16 aid = ecs.identity[i].actor_id;
                    if (victim == WORLD_ACTOR_CAP || aid < victim_id) {
                        victim = i;
                        victim_id = aid;
                    }
                }
                if (victim >= WORLD_ACTOR_CAP) break;
                despawn_item_actor(victim);
                --count;
            }
        }

        IO_NODISCARD inline bool spawn_item_actor(ge::item::Stack stack,
                                                  float x, float y, float z,
                                                  float vx, float vy, float vz,
                                                  io::u64 now_ms) noexcept {
            if (!world_actor_ecs) return false;
            ge::item::normalize(stack);
            if (ge::item::is_empty(stack)) return false;
            ActorEcs& ecs = *world_actor_ecs;
            const io::i32 slot = ecs.Spawn(ge::net::WORLD_ACTOR_MODEL_ITEM, ge::net::WORLD_ACTOR_MODE_ENTITY);
            if (slot < 0) return false;

            const io::u32 i = static_cast<io::u32>(slot);
            ecs.transform[i].x = x;
            ecs.transform[i].y = y;
            ecs.transform[i].z = z;
            ecs.velocity[i].x = vx;
            ecs.velocity[i].y = vy;
            ecs.velocity[i].z = vz;
            ecs.mob_state[i].logical = ge::ecs::MobLogicState::Idle;
            ecs.mob_state[i].net_state = static_cast<io::u8>(stack.id);
            ecs.mob_state[i].net_anim = static_cast<io::u8>(ge::item::freshness_band(stack));
            ecs.item_drop[i].stack = stack;
            ecs.item_drop[i].pile_count = stack.count;
            ecs.item_drop[i].grounded = false;
            ecs.item_drop[i].despawn_ms = WORLD_ITEM_DESPAWN_MS;
            ecs.anchor[i] = {};
            ecs.net_sync[i].active = true;
            ecs.net_sync[i].dirty = true;
            ecs.net_sync[i].last_update_ms = now_ms;
            ge::voxel::ChunkCoord cc{};
            io::u32 lx = 0u, ly = 0u, lz = 0u;
            ge::voxel::split_world_coord(floor_to_i32(x), floor_to_i32(y), floor_to_i32(z), cc, lx, ly, lz);
            enforce_world_item_chunk_cap(cc, i);
            return true;
        }

        inline void despawn_item_actor(io::u32 actor_index) noexcept {
            if (!world_actor_ecs || actor_index >= WORLD_ACTOR_CAP) return;
            world_actor_ecs->MarkInactive(actor_index);
        }

        IO_NODISCARD inline bool spawn_player_dropped_stack(io::u16 peer_index,
                                                            ge::item::Stack stack,
                                                            io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            const PeerState& p = peers[peer_index];
            if (!p.used || !p.has_auth) return false;
            ge::item::normalize(stack);
            if (ge::item::is_empty(stack)) return false;
            const float vx = static_cast<float>(p.move_dir_x) * 1.4f;
            const float vz = static_cast<float>(p.move_dir_z) * 1.4f;
            const float base_y = p.auth_y - PLAYER_EYE_TO_FEET + 0.28f;
            return spawn_item_actor(stack, p.auth_x, base_y, p.auth_z, vx, 1.0f, vz, now_ms);
        }

        inline void apply_use_selected(io::u16 peer_index, io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            PeerState& p = peers[peer_index];
            age_player_inventory(p, now_ms);
            const io::u8 selected = (p.inventory.selected_hotbar < ge::item::HOTBAR_SLOT_COUNT)
                ? p.inventory.selected_hotbar : 0u;
            ge::item::Stack& stack = p.inventory.hotbar[selected];
            ge::item::normalize(stack);
            if (ge::item::is_empty(stack) || !ge::item::is_consumable(stack.id)) return;
            p.eat_anim_until_ms = now_ms + 900u;
            p.action_flags |= ge::net::PLAYER_ACTION_FLAG_EAT;

            const io::u16 hunger_gain = ge::item::def(stack.id).hunger_gain;
            io::u32 hunger_now = static_cast<io::u32>(p.hunger) + hunger_gain;
            if (hunger_now > PLAYER_HUNGER_MAX) hunger_now = PLAYER_HUNGER_MAX;
            p.hunger = static_cast<io::u8>(hunger_now);

            if (ge::item::freshness_band(stack) == ge::item::FreshnessBand::Rotten) {
                const io::u16 poison_ms = ge::item::def(stack.id).poison_ms;
                if (poison_ms > 0u) {
                    p.poison_until_ms = now_ms + poison_ms;
                    p.next_poison_tick_ms = now_ms + PLAYER_POISON_TICK_MS;
                }
            }

            (void)ge::item::remove_one(stack);
            sync_inventory_if_needed(peer_index, now_ms, true);
            (void)send_health(peer_index, now_ms, 0.f, 0.f, io::UdpChan::Reliable);
        }

        inline void handle_inventory_action(io::u16 peer_index,
                                            const ge::net::InventoryAction& action,
                                            io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            PeerState& p = peers[peer_index];
            if (!p.used) return;
            ++stats.inventory_rx;
            age_player_inventory(p, now_ms);
            ensure_inventory_ward_tokens(p);

            if (action.action == ge::net::INVENTORY_ACTION_SELECT_HOTBAR) {
                if (action.src_index < ge::item::HOTBAR_SLOT_COUNT) {
                    p.inventory.selected_hotbar = action.src_index;
                    ensure_inventory_ward_tokens(p);
                    sync_inventory_if_needed(peer_index, now_ms, true);
                }
                return;
            }

            if (action.action == ge::net::INVENTORY_ACTION_MOVE) {
                if (ge::item::move_between_player_slots(p.inventory,
                                                        action.src_region, action.src_index,
                                                        action.dst_region, action.dst_index)) {
                    ensure_inventory_ward_tokens(p);
                    sync_inventory_if_needed(peer_index, now_ms, true);
                }
                return;
            }

            if (action.action == ge::net::INVENTORY_ACTION_USE_SELECTED) {
                apply_use_selected(peer_index, now_ms);
                return;
            }

            if (action.action == ge::net::INVENTORY_ACTION_LEFT_CLICK) {
                if (ge::item::left_click(p.inventory, action.src_region, action.src_index)) {
                    ensure_inventory_ward_tokens(p);
                    sync_inventory_if_needed(peer_index, now_ms, true);
                }
                return;
            }

            if (action.action == ge::net::INVENTORY_ACTION_RIGHT_CLICK) {
                if (ge::item::right_click(p.inventory, action.src_region, action.src_index)) {
                    ensure_inventory_ward_tokens(p);
                    sync_inventory_if_needed(peer_index, now_ms, true);
                }
                return;
            }

            if (action.action == ge::net::INVENTORY_ACTION_DROP_SLOT_STACK ||
                action.action == ge::net::INVENTORY_ACTION_DROP_SLOT_ONE ||
                action.action == ge::net::INVENTORY_ACTION_DROP_CURSOR_STACK ||
                action.action == ge::net::INVENTORY_ACTION_DROP_CURSOR_ONE) {
                ge::item::Stack dropped{};
                bool ok = false;
                if (action.action == ge::net::INVENTORY_ACTION_DROP_SLOT_STACK) {
                    ok = ge::item::drop_from_slot(p.inventory, action.src_region, action.src_index, 0u, dropped);
                } else if (action.action == ge::net::INVENTORY_ACTION_DROP_SLOT_ONE) {
                    ok = ge::item::drop_from_slot(p.inventory, action.src_region, action.src_index, 1u, dropped);
                } else if (action.action == ge::net::INVENTORY_ACTION_DROP_CURSOR_STACK) {
                    ok = ge::item::drop_from_slot(p.inventory, ge::item::SlotRegion::Cursor, 0u, 0u, dropped);
                } else {
                    ok = ge::item::drop_from_slot(p.inventory, ge::item::SlotRegion::Cursor, 0u, 1u, dropped);
                }
                if (ok) {
                    (void)spawn_player_dropped_stack(peer_index, dropped, now_ms);
                    ensure_inventory_ward_tokens(p);
                    sync_inventory_if_needed(peer_index, now_ms, true);
                }
            }
        }

        IO_NODISCARD inline bool apply_ward_config_click(PeerState& p,
                                                         WardInstance& cfg,
                                                         io::u8 slot_index,
                                                         bool right_click) noexcept {
            if (slot_index >= WARD_SLOT_COUNT) return false;
            if (slot_index >= cfg.slots_available) return false;

            ge::item::Stack& slot = cfg.spells[slot_index];
            ge::item::Stack& cursor = p.inventory.cursor;
            ge::item::normalize(slot);
            ge::item::normalize(cursor);
            if (!ge::item::is_empty(slot) && !is_spell_item_id(slot.id))
                ge::item::clear(slot);

            if (!right_click) {
                if (ge::item::is_empty(cursor)) {
                    if (ge::item::is_empty(slot)) return false;
                    cursor = slot;
                    ge::item::clear(slot);
                    return true;
                }
                if (!is_spell_item_id(cursor.id))
                    return false;
                if (ge::item::is_empty(slot)) {
                    slot = cursor;
                    ge::item::clear(cursor);
                    return true;
                }
                if (ge::item::can_stack_together(slot, cursor)) {
                    const io::u16 limit = ge::item::max_stack(slot.id);
                    if (slot.count >= limit) return false;
                    io::u16 add = static_cast<io::u16>(limit - slot.count);
                    if (add > cursor.count) add = cursor.count;
                    slot.count = static_cast<io::u16>(slot.count + add);
                    cursor.count = static_cast<io::u16>(cursor.count - add);
                    if (cursor.count == 0u)
                        ge::item::clear(cursor);
                    return add > 0u;
                }
                const ge::item::Stack tmp = slot;
                slot = cursor;
                cursor = tmp;
                return true;
            }

            if (ge::item::is_empty(cursor)) {
                if (ge::item::is_empty(slot)) return false;
                const io::u16 take = static_cast<io::u16>((slot.count + 1u) / 2u);
                cursor = slot;
                cursor.count = take;
                slot.count = static_cast<io::u16>(slot.count - take);
                ge::item::normalize(slot);
                return true;
            }
            if (!is_spell_item_id(cursor.id))
                return false;
            if (ge::item::is_empty(slot)) {
                slot = ge::item::make_stack(cursor.id, 1u, cursor.freshness);
                cursor.count = static_cast<io::u16>(cursor.count - 1u);
                if (cursor.count == 0u)
                    ge::item::clear(cursor);
                return true;
            }
            if (!ge::item::can_stack_together(slot, cursor))
                return false;
            const io::u16 limit = ge::item::max_stack(slot.id);
            if (slot.count >= limit)
                return false;
            ++slot.count;
            cursor.count = static_cast<io::u16>(cursor.count - 1u);
            if (cursor.count == 0u)
                ge::item::clear(cursor);
            return true;
        }

        inline void handle_ward_config_action(io::u16 peer_index,
                                              const ge::net::WardConfigActionSample& action,
                                              io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return;
            PeerState& p = peers[peer_index];
            if (!p.used) return;
            age_player_inventory(p, now_ms);
            ensure_inventory_ward_tokens(p);
            const io::u16 token = action.ward_instance;
            if (token == 0u) return;
            if (!inventory_has_ward_token(p, token)) return;
            WardInstance* cfg = find_ward_instance(p, token);
            if (!cfg || !cfg->active) return;
            if (action.ward_slot >= cfg->slots_available) return;
            if (!apply_ward_config_click(p, *cfg, action.ward_slot, action.right_click))
                return;
            ensure_inventory_ward_tokens(p);
            sync_inventory_if_needed(peer_index, now_ms, true);
            (void)send_ward_config_state_to_peer(peer_index, token, now_ms, io::UdpChan::Reliable);
        }

        IO_NODISCARD inline bool dagger_can_break_block(ge::voxel::BlockId block_id) const noexcept {
            return ge::build::dagger_can_break(block_build_profile, block_id);
        }

        IO_NODISCARD inline bool held_can_break_block(const ge::item::Stack& held,
                                                      ge::voxel::BlockId block_id) const noexcept {
            if (ge::item::is_empty(held) || block_id == ge::voxel::BlockId::Air)
                return false;
            if (held.id == ge::item::Id::RustyDagger)
                return dagger_can_break_block(block_id);
            return ge::item::def(held.id).category == ge::item::Category::SpellingWards;
        }

        IO_NODISCARD inline bool apply_player_block_edit(io::u16 peer_index,
                                                         const ge::net::BlockEdit& requested_edit,
                                                         io::u64 now_ms) noexcept {
            if (!peers || peer_index >= static_cast<io::u16>(io::MAX_PEERS)) return false;
            PeerState& p = peers[peer_index];
            if (!p.used) return false;
            age_player_inventory(p, now_ms);

            ge::voxel::ChunkCoord target_cc{};
            io::u32 target_lx = 0u, target_ly = 0u, target_lz = 0u;
            ge::voxel::split_world_coord(requested_edit.wx, requested_edit.wy, requested_edit.wz,
                                         target_cc, target_lx, target_ly, target_lz);
            if (!ensure_world_chunk(target_cc))
                return false;

            const ge::voxel::BlockId prev_id = world.get_world_block(requested_edit.wx, requested_edit.wy, requested_edit.wz);
            const ge::voxel::BlockId next_id = (requested_edit.block_id < ge::voxel::BLOCK_COUNT)
                ? static_cast<ge::voxel::BlockId>(requested_edit.block_id)
                : ge::voxel::BlockId::Air;

            ge::net::BlockEdit edit = requested_edit;
            const io::u8 selected = (p.inventory.selected_hotbar < ge::item::HOTBAR_SLOT_COUNT)
                ? p.inventory.selected_hotbar : 0u;
            ge::item::Stack& held = p.inventory.hotbar[selected];
            ge::item::normalize(held);
            const bool held_is_magic = (!ge::item::is_empty(held) &&
                                        ge::item::def(held.id).category == ge::item::Category::SpellingWards);

            if (next_id == ge::voxel::BlockId::Air) {
                if (prev_id == ge::voxel::BlockId::Air) return false;
                if (!p.use_fly) {
                    if (!held_can_break_block(held, prev_id))
                        return false;
                }
                if (prev_id == ge::voxel::BlockId::Log)
                    (void)try_begin_tree_fall_on_log_break(requested_edit.wx, requested_edit.wy, requested_edit.wz, now_ms);
                if (!apply_block_edit_world(edit)) return false;

                const ge::item::Id drop_id = ge::item::block_drop_for(prev_id);
                if (drop_id != ge::item::Id::None) {
                    ge::item::Stack drop = ge::item::make_stack(drop_id, 1u);
                    const float jitter = static_cast<float>((requested_edit.wx * 13 + requested_edit.wz * 7) & 3) * 0.05f;
                    if (!spawn_item_actor(drop,
                                          static_cast<float>(requested_edit.wx) + 0.5f,
                                          static_cast<float>(requested_edit.wy) + 0.35f,
                                          static_cast<float>(requested_edit.wz) + 0.5f,
                                          0.12f + jitter, 2.8f, -0.10f - jitter,
                                          now_ms)) {
                        (void)give_item_to_peer(peer_index, drop, now_ms);
                    }
                }
                if (held_is_magic && !p.use_fly) {
                    region_note_ward_interaction(requested_edit.wx, requested_edit.wy, requested_edit.wz, true, now_ms);
                }
                sync_inventory_if_needed(peer_index, now_ms, true);
                return true;
            }

            if (prev_id != ge::voxel::BlockId::Air) return false;
            if (ge::item::is_empty(held)) return false;
            if (!ge::item::is_placeable(held.id)) return false;
            if (ge::item::place_block(held.id) != next_id) return false;
            if (!apply_block_edit_world(edit)) return false;
            if (!p.use_fly)
                (void)ge::item::remove_one(held);
            if (held_is_magic && !p.use_fly) {
                region_note_ward_interaction(requested_edit.wx, requested_edit.wy, requested_edit.wz, false, now_ms);
            }
            sync_inventory_if_needed(peer_index, now_ms, true);
            return true;
        }
