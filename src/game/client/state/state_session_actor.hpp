    inline void ResetDtHistory() noexcept {
        frame.dt_history.clear();
    }

    inline void ClearSessionText() noexcept {
        session.endpoint_len = 0;
        session.endpoint_utf8[0] = '\0';
        session.server_name_len = 0;
        session.server_name_utf8[0] = '\0';
    }

    inline void SetSessionText(io::char_view endpoint, io::char_view server_name) noexcept {
        session.endpoint_len = 0;
        for (io::usize i = 0; i < endpoint.size() && i < sizeof(session.endpoint_utf8) - 1; ++i)
            session.endpoint_utf8[session.endpoint_len++] = endpoint[i];
        session.endpoint_utf8[session.endpoint_len] = '\0';

        session.server_name_len = 0;
        for (io::usize i = 0; i < server_name.size() && i < sizeof(session.server_name_utf8) - 1; ++i)
            session.server_name_utf8[session.server_name_len++] = server_name[i];
        session.server_name_utf8[session.server_name_len] = '\0';
    }

    IO_NODISCARD inline io::char_view SessionEndpointView() const noexcept {
        return io::char_view{ session.endpoint_utf8, session.endpoint_len };
    }

    IO_NODISCARD inline io::char_view SessionServerNameView() const noexcept {
        return io::char_view{ session.server_name_utf8, session.server_name_len };
    }

    inline void EnterSingleplayer() noexcept {
        CancelRebind();
        ResetMovementModes();
        session.mode = SessionMode::Singleplayer;
        SetSessionText("127.0.0.1:25565", "Singleplayer");
        screen = ScreenState::Connecting;
        setCursorVisible(true);
        frame.first_mouse_sample = true;
        ClearChunkWorld();
        if (!BeginConnectToEndpoint("127.0.0.1", 25565u)) {
#ifdef _DEBUG
            io::out << "[net] failed to begin singleplayer connect\n";
#endif
        }
    }

    inline void EnterMultiplayer() noexcept {
        CancelRebind();
        ResetMovementModes();
        SyncServerFormFromSelected();
        screen = ScreenState::Multiplayer;
        setCursorVisible(true);
        frame.first_mouse_sample = true;
    }

    inline void EnterInGameConnected() noexcept {
        if (!InitChunkJobSlots()) {
#ifdef _DEBUG
            io::out << "[chunk] failed to init job slots (connected)\n";
#endif
            return;
        }
        if (!InitChunkWorld()) {
#ifdef _DEBUG
            io::out << "[chunk] failed to init world slots (connected)\n";
#endif
            return;
        }
        ResetPlayerState();
        ResetChatLog();
        dev_hud_visible = false;
        player_hud_visible = false;
        ResetHotbarHintTracking();
        world_time_start_ms = io::monotonic_ms();
        screen = ScreenState::InGame;
        setCursorVisible(false);
        frame.first_mouse_sample = true;
    }

    inline void ReturnToMainMenu() noexcept {
        StopConnect();
        ResetMovementModes();
        ResetPlayerState();
        ClearChunkWorld();
        ResetChatLog();
        screen = ScreenState::MainMenu;
        session.mode = SessionMode::None;
        ClearSessionText();
        setCursorVisible(true);
        frame.first_mouse_sample = true;
    }

    static inline io::u16 ParsePortOrDefault(io::char_view port_text, io::u16 fallback) noexcept {
        if (port_text.empty()) return fallback;
        io::u32 value = 0u;
        for (io::usize i = 0; i < port_text.size(); ++i) {
            const char ch = port_text[i];
            if (ch < '0' || ch > '9') return fallback;
            value = value * 10u + static_cast<io::u32>(ch - '0');
            if (value > 65535u) return fallback;
        }
        if (value == 0u) return fallback;
        return static_cast<io::u16>(value);
    }

    static inline void CopyTextField(io::char_view src, char* dst, io::usize dst_cap, io::usize& out_len) noexcept {
        out_len = 0;
        if (dst_cap == 0u) return;
        for (io::usize i = 0; i < src.size() && out_len + 1u < dst_cap; ++i)
            dst[out_len++] = src[i];
        dst[out_len] = '\0';
    }

    IO_NODISCARD inline io::char_view ServerNameInputView() const noexcept {
        return io::char_view{ server_name_input, server_name_input_len };
    }

    IO_NODISCARD inline io::char_view ServerIpInputView() const noexcept {
        return io::char_view{ server_ip_input, server_ip_input_len };
    }

    IO_NODISCARD inline io::char_view ServerPortInputView() const noexcept {
        return io::char_view{ server_port_input, server_port_input_len };
    }

    inline void ClampServerFormLens() noexcept {
        if (server_name_input_len > 32u) server_name_input_len = 32u;
        if (server_ip_input_len > 48u) server_ip_input_len = 48u;
        if (server_port_input_len > 7u) server_port_input_len = 7u;
        server_name_input[server_name_input_len] = '\0';
        server_ip_input[server_ip_input_len] = '\0';
        server_port_input[server_port_input_len] = '\0';
    }

    inline void SyncServerFormFromSelected() noexcept {
        if (server_list.empty()) {
            server_name_input_len = 0;
            server_name_input[0] = '\0';
            server_ip_input_len = 0;
            server_ip_input[0] = '\0';
            static constexpr char k_port[] = "25565";
            CopyTextField(io::char_view{ k_port, sizeof(k_port) - 1 }, server_port_input, sizeof(server_port_input), server_port_input_len);
            return;
        }

        if (server_selected_index >= server_list.size())
            server_selected_index = 0;

        const ge::ServerListEntry& e = server_list[server_selected_index];
        CopyTextField(io::char_view{ e.name_utf8 }, server_name_input, sizeof(server_name_input), server_name_input_len);
        CopyTextField(io::char_view{ e.ip_utf8 }, server_ip_input, sizeof(server_ip_input), server_ip_input_len);

        io::StackOut<16> port_text{};
        port_text << static_cast<io::u32>(e.port);
        CopyTextField(port_text.view(), server_port_input, sizeof(server_port_input), server_port_input_len);
        ClampServerFormLens();
    }

    inline bool SaveServerList() noexcept {
        if (server_list.empty()) {
            if (!server_list.resize(1)) return false;
            ge::set_default_server_entry(server_list[0]);
        }
        return ge::save_server_list_binary(io::view<const ge::ServerListEntry>{ server_list.data(), server_list.size() });
    }

    inline bool LoadServerList() noexcept {
        const bool ok = ge::ensure_server_list_binary(server_list, server_list_meta);
        if (!ok) return false;
        if (server_list.empty()) {
            if (!server_list.resize(1)) return false;
            ge::set_default_server_entry(server_list[0]);
        }
        if (server_selected_index >= server_list.size())
            server_selected_index = 0;
        return true;
    }

    inline void UpdateSelectedServerFromForm() noexcept {
        if (server_list.empty() || server_selected_index >= server_list.size()) return;
        ClampServerFormLens();
        ge::ServerListEntry& e = server_list[server_selected_index];
        for (io::usize i = 0; i < sizeof(e.name_utf8); ++i)
            e.name_utf8[i] = (i < server_name_input_len) ? server_name_input[i] : '\0';
        for (io::usize i = 0; i < sizeof(e.ip_utf8); ++i)
            e.ip_utf8[i] = (i < server_ip_input_len) ? server_ip_input[i] : '\0';
        e.port = ParsePortOrDefault(ServerPortInputView(), 25565u);
        ge::sanitize_server_entry(e);
        SyncServerFormFromSelected();
        (void)SaveServerList();
    }

    inline void AddServerFromForm() noexcept {
        ClampServerFormLens();
        const io::usize old_size = server_list.size();
        if (!server_list.resize(old_size + 1u)) return;
        ge::ServerListEntry& e = server_list[old_size];
        e = {};
        for (io::usize i = 0; i < sizeof(e.name_utf8); ++i)
            e.name_utf8[i] = (i < server_name_input_len) ? server_name_input[i] : '\0';
        for (io::usize i = 0; i < sizeof(e.ip_utf8); ++i)
            e.ip_utf8[i] = (i < server_ip_input_len) ? server_ip_input[i] : '\0';
        e.port = ParsePortOrDefault(ServerPortInputView(), 25565u);
        ge::sanitize_server_entry(e);
        server_selected_index = old_size;
        SyncServerFormFromSelected();
        (void)SaveServerList();
    }

    inline void RemoveSelectedServer() noexcept {
        if (server_list.empty() || server_selected_index >= server_list.size()) return;
        const io::usize last = server_list.size() - 1u;
        for (io::usize i = server_selected_index; i < last; ++i)
            server_list[i] = server_list[i + 1u];
        server_list.pop_back();
        if (server_list.empty()) {
            if (server_list.resize(1))
                ge::set_default_server_entry(server_list[0]);
            server_selected_index = 0;
        } else if (server_selected_index >= server_list.size()) {
            server_selected_index = server_list.size() - 1u;
        }
        SyncServerFormFromSelected();
        (void)SaveServerList();
    }

    inline void MoveSelectedServerUp() noexcept {
        if (server_list.empty() || server_selected_index == 0u || server_selected_index >= server_list.size()) return;
        const io::usize i = server_selected_index;
        ge::ServerListEntry tmp = server_list[i - 1u];
        server_list[i - 1u] = server_list[i];
        server_list[i] = tmp;
        server_selected_index = i - 1u;
        (void)SaveServerList();
    }

    inline void MoveSelectedServerDown() noexcept {
        if (server_list.empty() || server_selected_index + 1u >= server_list.size()) return;
        const io::usize i = server_selected_index;
        ge::ServerListEntry tmp = server_list[i + 1u];
        server_list[i + 1u] = server_list[i];
        server_list[i] = tmp;
        server_selected_index = i + 1u;
        (void)SaveServerList();
    }

    IO_NODISCARD inline io::char_view NetStateText() const noexcept {
        const io::u32 state = net_state.load();
        if (state == 2u) return "connected";
        if (state == 1u) return "connecting";
        if (state == 3u) return "failed";
        return "idle";
    }

    static inline io::usize BlockFaceUvIndex(io::u16 bid, io::u8 face) noexcept {
        return static_cast<io::usize>(bid) * static_cast<io::usize>(FACE_INDEX_COUNT) + static_cast<io::usize>(face);
    }

    inline BlockFaceUv& BlockUvRef(io::u16 bid, io::u8 face) noexcept {
        return block_face_uv[BlockFaceUvIndex(bid, face)];
    }

    inline const BlockFaceUv& BlockUvRef(io::u16 bid, io::u8 face) const noexcept {
        return block_face_uv[BlockFaceUvIndex(bid, face)];
    }

    static inline io::i32 abs_i32(io::i32 v) noexcept {
        return v < 0 ? -v : v;
    }

    inline void ResetSandLerpVisuals() noexcept {
        sand_source_event_cursor = 0u;
        sand_lerp_visual_cursor = 0u;
        if (sand_source_events)
            for (io::u32 i = 0; i < SAND_SOURCE_EVENT_CAP; ++i)
                sand_source_events[i] = {};
        if (sand_lerp_visuals)
            for (io::u32 i = 0; i < SAND_LERP_VISUAL_CAP; ++i)
                sand_lerp_visuals[i] = {};
    }

    inline void PushSandSourceEvent(io::i32 wx, io::i32 wy, io::i32 wz, io::u64 now_ms) noexcept {
        if (!sand_source_events) return;
        SandSourceEvent& e = sand_source_events[sand_source_event_cursor];
        e.used = true;
        e.wx = wx;
        e.wy = wy;
        e.wz = wz;
        e.at_ms = now_ms;
        sand_source_event_cursor = (sand_source_event_cursor + 1u) % SAND_SOURCE_EVENT_CAP;
    }

    IO_NODISCARD inline bool ConsumeSandSourceForDestination(io::i32 dst_wx, io::i32 dst_wy, io::i32 dst_wz,
                                                             io::i32& out_src_wx, io::i32& out_src_wy, io::i32& out_src_wz,
                                                             io::u64 now_ms) noexcept {
        if (!sand_source_events) return false;
        static constexpr io::u64 MAX_AGE_MS = 300u;
        for (io::u32 i = 0; i < SAND_SOURCE_EVENT_CAP; ++i) {
            const io::u32 idx = (sand_source_event_cursor + SAND_SOURCE_EVENT_CAP - 1u - i) % SAND_SOURCE_EVENT_CAP;
            SandSourceEvent& e = sand_source_events[idx];
            if (!e.used) continue;
            if (now_ms > e.at_ms + MAX_AGE_MS) {
                e.used = false;
                continue;
            }
            if (e.wy != dst_wy + 1) continue;
            if (abs_i32(e.wx - dst_wx) > 1) continue;
            if (abs_i32(e.wz - dst_wz) > 1) continue;
            out_src_wx = e.wx;
            out_src_wy = e.wy;
            out_src_wz = e.wz;
            e.used = false;
            return true;
        }
        return false;
    }

    inline void SpawnSandLerpVisual(io::i32 src_wx, io::i32 src_wy, io::i32 src_wz,
                                    io::i32 dst_wx, io::i32 dst_wy, io::i32 dst_wz,
                                    io::u64 now_ms) noexcept {
        if (!sand_lerp_visuals) return;
        // Chain consecutive sand moves into a single visual to avoid "stretching trails".
        for (io::u32 i = 0; i < SAND_LERP_VISUAL_CAP; ++i) {
            SandLerpVisual& v = sand_lerp_visuals[i];
            if (!v.active) continue;
            if (static_cast<io::i32>(v.dst_x) != src_wx) continue;
            if (static_cast<io::i32>(v.dst_y) != src_wy) continue;
            if (static_cast<io::i32>(v.dst_z) != src_wz) continue;

            io::u32 elapsed_ms = 0u;
            if (now_ms >= v.start_ms) {
                const io::u64 delta = now_ms - v.start_ms;
                elapsed_ms = (delta > 0xFFFFFFFFull) ? 0xFFFFFFFFu : static_cast<io::u32>(delta);
            }
            if (elapsed_ms >= v.duration_ms) {
                v.active = false;
                break;
            }

            float t = static_cast<float>(elapsed_ms) / static_cast<float>(v.duration_ms);
            t = clampf(t, 0.f, 1.f);
            t = t * t * (3.f - 2.f * t);
            const float cur_x = v.src_x + (v.dst_x - v.src_x) * t;
            const float cur_y = v.src_y + (v.dst_y - v.src_y) * t;
            const float cur_z = v.src_z + (v.dst_z - v.src_z) * t;

            v.src_x = cur_x;
            v.src_y = cur_y;
            v.src_z = cur_z;
            v.dst_x = static_cast<float>(dst_wx);
            v.dst_y = static_cast<float>(dst_wy);
            v.dst_z = static_cast<float>(dst_wz);
            v.start_ms = now_ms;
            v.duration_ms = 140u;
            v.active = true;
            return;
        }

        SandLerpVisual& v = sand_lerp_visuals[sand_lerp_visual_cursor];
        v.active = true;
        v.src_x = static_cast<float>(src_wx);
        v.src_y = static_cast<float>(src_wy);
        v.src_z = static_cast<float>(src_wz);
        v.dst_x = static_cast<float>(dst_wx);
        v.dst_y = static_cast<float>(dst_wy);
        v.dst_z = static_cast<float>(dst_wz);
        v.start_ms = now_ms;
        v.duration_ms = 140u;
        sand_lerp_visual_cursor = (sand_lerp_visual_cursor + 1u) % SAND_LERP_VISUAL_CAP;
    }

    inline void UpdateSandLerpFromBlockEdit(io::i32 wx, io::i32 wy, io::i32 wz, ge::voxel::BlockId next_id) noexcept {
        if (!sand_source_events || !sand_lerp_visuals) return;
        if (!chunk_world_ready) return;
        const ge::voxel::BlockId prev_id = voxel_world.get_world_block(wx, wy, wz);
        const io::u64 now_ms = io::monotonic_ms();

        if (prev_id == ge::voxel::BlockId::Sand && next_id == ge::voxel::BlockId::Air) {
            PushSandSourceEvent(wx, wy, wz, now_ms);
            return;
        }

        if (next_id == ge::voxel::BlockId::Sand && prev_id != ge::voxel::BlockId::Sand) {
            io::i32 src_wx = 0, src_wy = 0, src_wz = 0;
            if (ConsumeSandSourceForDestination(wx, wy, wz, src_wx, src_wy, src_wz, now_ms))
                SpawnSandLerpVisual(src_wx, src_wy, src_wz, wx, wy, wz, now_ms);
        }
    }

    inline void ResetWorldActors() noexcept {
        if (!world_actor_ecs) return;
        world_actor_ecs->Reset();
    }

    IO_NODISCARD inline io::i32 FindWorldActorSlot(io::u16 actor_id) const noexcept {
        if (!world_actor_ecs || actor_id == 0u) return -1;
        return world_actor_ecs->FindByActorId(actor_id);
    }

    IO_NODISCARD inline io::i32 FindFreeWorldActorSlot() const noexcept {
        if (!world_actor_ecs) return -1;
        return world_actor_ecs->FindFree();
    }

    inline void ApplyIncomingWorldActor(const ge::net::WorldActorSample& actor) noexcept {
        if (!world_actor_ecs || actor.actor_id == 0u) return;
        ActorEcs& ecs = *world_actor_ecs;
        const bool actor_active = (actor.flags & ge::net::WORLD_ACTOR_FLAG_ACTIVE) != 0u;
        io::i32 slot = FindWorldActorSlot(actor.actor_id);
        bool created = false;
        if (!actor_active) {
            if (slot >= 0) ecs.Erase(static_cast<io::u32>(slot));
            return;
        }

        if (slot < 0) {
            slot = ecs.Spawn(actor.model, actor.mode, actor.actor_id);
            if (slot < 0) return;
            created = true;
        } else {
            const io::u32 i = static_cast<io::u32>(slot);
            if (ecs.alive[i] == 0u) return;
        }

        const io::u32 i = static_cast<io::u32>(slot);
        const bool first_update = created;
        const io::u8 next_anim = actor.anim;
        if (first_update) {
            ecs.animator[i].clip_time = 0.f;
            ecs.animator[i].current_clip = ENTITY_CLIP_INVALID;
            ecs.animator[i].next_clip = ENTITY_CLIP_INVALID;
        } else if (ecs.mob_state[i].net_anim != next_anim) {
            ecs.animator[i].clip_time = 0.f;
        }

        ecs.alive[i] = 1u;
        ecs.identity[i].actor_id = actor.actor_id;
        ecs.identity[i].model = actor.model;
        ecs.identity[i].mode = actor.mode;
        ecs.mob_state[i].net_state = actor.state;
        ecs.mob_state[i].net_anim = next_anim;
        ecs.transform[i].x = actor.x;
        ecs.transform[i].y = actor.y;
        ecs.transform[i].z = actor.z;
        if (actor.model == ge::net::WORLD_ACTOR_MODEL_ITEM ||
            actor.model == ge::net::WORLD_ACTOR_MODEL_SPELL) {
            ge::item::Id sid = static_cast<ge::item::Id>(actor.state);
            if (!ge::item::valid(sid))
                sid = ge::item::Id::None;
            ecs.item_drop[i].stack.id = sid;
            ecs.item_drop[i].stack.count = 1u;
            if (actor.model == ge::net::WORLD_ACTOR_MODEL_ITEM) {
                switch (static_cast<ge::item::FreshnessBand>(actor.anim)) {
                case ge::item::FreshnessBand::Rotten: ecs.item_drop[i].stack.freshness = 100u; break;
                case ge::item::FreshnessBand::Stale: ecs.item_drop[i].stack.freshness = 500u; break;
                default: ecs.item_drop[i].stack.freshness = ge::item::FRESHNESS_MAX; break;
                }
                ecs.item_drop[i].grounded = (actor.flags & ge::net::WORLD_ACTOR_FLAG_GROUNDED) != 0u;
            } else {
                ecs.item_drop[i].stack.freshness = ge::item::FRESHNESS_MAX;
                ecs.item_drop[i].grounded = false;
            }
            ge::item::normalize(ecs.item_drop[i].stack);
            ecs.item_drop[i].pile_count = ecs.item_drop[i].stack.count;
        } else {
            ecs.item_drop[i] = {};
        }
        ecs.net_sync[i].active = true;
        ecs.net_sync[i].dirty = false;
        ecs.net_sync[i].last_update_ms = io::monotonic_ms();
        ecs.animator[i].playback_speed = 1.f;
        ecs.animator[i].loop = true;
        ecs.render_model[i].visible = true;
    }

