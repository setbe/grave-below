#pragma once

    static inline float PostFxClamp01(float v) noexcept {
        if (v < 0.f) return 0.f;
        if (v > 1.f) return 1.f;
        return v;
    }

    inline void DrawPlayerPostFx() noexcept {
        float black_strength = 0.06f;
        float red_strength = 0.f;
        hi::UiColor map_tint = ge::ui::Color(0.f, 0.f, 0.f, 0.f);
        float map_tint_strength = 0.f;
        float region_mana = 1.f;
        float region_instability = 0.f;
        float region_decay = 0.f;
        GetActiveRegionVisual(region_mana, region_instability, region_decay);
        if (player_hp <= 33.f) {
            const float low_hp = PostFxClamp01((33.f - player_hp) / 33.f);
            black_strength += low_hp * 0.20f;
            red_strength = low_hp * 0.55f;
        }
        if (player_dead) {
            black_strength = 0.95f;
            red_strength = 0.f;
        }
        if (head_overlay_active && block_map_tint_ready && block_map_tint) {
            const io::u16 bid = ge::voxel::block_index(head_overlay_block);
            if (bid < ge::voxel::BLOCK_COUNT) {
                map_tint = block_map_tint[bid];
                map_tint_strength = PostFxClamp01(map_tint.a);
            }
        }
        if (map_tint_strength < 0.08f) {
            map_tint = ge::ui::Color(0.64f, 0.66f, 0.68f, 0.18f);
            map_tint_strength = PostFxClamp01(region_decay * 0.24f);
        }

        post_effect_shader.Use();
        if (post_effect_uniforms.u_black_strength >= 0) gl::Uniform1f(post_effect_uniforms.u_black_strength, black_strength);
        if (post_effect_uniforms.u_red_strength >= 0) gl::Uniform1f(post_effect_uniforms.u_red_strength, red_strength);
        if (post_effect_uniforms.u_dead_strength >= 0) gl::Uniform1f(post_effect_uniforms.u_dead_strength, player_dead ? 1.f : 0.f);
        if (post_effect_uniforms.u_map_tint_rg >= 0) gl::Uniform2f(post_effect_uniforms.u_map_tint_rg, map_tint.r, map_tint.g);
        if (post_effect_uniforms.u_map_tint_b >= 0) gl::Uniform1f(post_effect_uniforms.u_map_tint_b, map_tint.b);
        if (post_effect_uniforms.u_map_tint_strength >= 0) gl::Uniform1f(post_effect_uniforms.u_map_tint_strength, map_tint_strength);
        if (post_effect_uniforms.u_region_decay >= 0) gl::Uniform1f(post_effect_uniforms.u_region_decay, PostFxClamp01(region_decay));
        if (post_effect_uniforms.u_region_instability >= 0) gl::Uniform1f(post_effect_uniforms.u_region_instability, PostFxClamp01(region_instability));
        sky_vao.bind();
        gl::Enable(gl::Capability::Blend);
        gl::BlendFunc(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha);
        gl::DrawArrays(gl::PrimitiveMode::Triangles, 0, 3);
        gl::Disable(gl::Capability::Blend);
    }

    inline void RenderInGameHud(float dt) noexcept {
        frame.dt_history.push(dt);
        UpdateHotbarHintTracking();
        const bool show_dev_hud = dev_hud_visible;
        const bool show_player_hud = player_hud_visible;
        DrawPlayerPostFx();

        if (show_player_hud) {
    const bool godmode_active = use_fly;
            io::StackOut<640> hud{};
            hud.reset();
            hud << "Survival HUD\n";
            WriteSessionContext(hud);
            hud << "\nMove: ";
            AppendBindingText(hud, Binding(Action::MoveForward));
            hud << "/";
            AppendBindingText(hud, Binding(Action::MoveLeft));
            hud << "/";
            AppendBindingText(hud, Binding(Action::MoveBackward));
            hud << "/";
            AppendBindingText(hud, Binding(Action::MoveRight));
            hud << "\nUp: ";
            AppendBindingText(hud, Binding(Action::MoveUp));
            hud << "  Down: ";
            AppendBindingText(hud, Binding(Action::MoveDown));
            hud << "\nSprint: ";
            AppendBindingText(hud, Binding(Action::Sprint));
            hud << "  Sneak: ";
            AppendBindingText(hud, Binding(Action::Sneak));
            hud << "  Crawl: ";
            AppendBindingText(hud, Binding(Action::Crawl));
            hud << "\nQuick slots: ";
            AppendBindingText(hud, Binding(Action::QuickSlot1));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot2));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot3));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot4));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot5));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot6));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot7));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot8));
            hud << " ";
            AppendBindingText(hud, Binding(Action::QuickSlot9));
            hud << "\nInventory: ";
            AppendBindingText(hud, Binding(Action::ToggleInventory));
            hud << "\nChat: enter or /";
            hud << "\nMode: " << (godmode_active ? "godmode" : "survival");
            hud << "\nHP: " << io::to_u32(player_hp + 0.5f) << "/100";
            hud << "  Hunger: " << static_cast<io::u32>(player_hunger) << "/255";
            if (player_hunger <= 26u) hud << "  sprint: disabled";
            if (player_dead) hud << "  DEAD";
            const io::u32 ping_ms = net_ping_ms.load();
            if (net_state.load() == 2u) hud << "\nPing: " << ping_ms << " ms";
            else hud << "\nPing: n/a ms";
            float region_mana = 1.f, region_instability = 0.f, region_decay = 0.f;
            GetActiveRegionVisual(region_mana, region_instability, region_decay);
            hud << "\nRegion: mana=" << io::to_u32(region_mana * 100.f + 0.5f) << "%"
                << " instability=" << io::to_u32(region_instability * 100.f + 0.5f) << "%"
                << " decay=" << io::to_u32(region_decay * 100.f + 0.5f) << "%";
    hud << "\nCursor: f1  Dev HUD: f2  Godmode: f3";
            hud << "\nPlayer HUD: f10  Fullscreen: f11  VSync: f12";

            hi::TextDraw td = ge::ui::TextRegular(world_atlas, hud.view());
            td.dock = hi::TextDock::TopL;
            td.x = 12.f;
            td.y = 12.f;
            td.scale = 0.78f;
            td.line_height = 0.8f;
            DrawText(td);
        }

        if (show_dev_hud) {
            io::StackOut<960> hud{};
            hud.reset();
            hud << "Developer HUD\n";
            WriteSessionContext(hud);
    hud << "\nMode flags: fly=" << (use_fly ? "true" : "false")
        << " noclip=" << (use_noclip ? "true" : "false");
            hud << "\nHP=" << io::to_u32(player_hp + 0.5f)
                << " hunger=" << static_cast<io::u32>(player_hunger)
                << " grounded=" << (player_grounded ? "true" : "false");
            if (player_last_fall_damage > 0.f) {
                hud << "\nLast fall damage: " << io::to_u32(player_last_fall_damage + 0.5f)
                    << " (" << io::to_u32(player_last_fall_blocks + 0.5f) << " blocks)";
            }
            hud << "\nRender distance: " << render_distance_chunks << " chunks";
            hud << "\nWorkers: 1/1/" << mesh_worker_threads
                << " (main/audio/mesh) of " << hw_threads << " logical cores";
            hud << "\nChunk meshes visible: " << chunk_meshes_visible
                << " / loaded " << chunk_meshes.size()
                << " / culled " << chunk_meshes_culled;
            hud << "\nChunk jobs: submitted=" << chunk_jobs_submitted
                << " completed=" << chunk_jobs_completed
                << " failed=" << chunk_jobs_failed;
            hud << "\nChunk network: received=" << net_received_chunks.load();
            hud << "\nLast chunk build: vertices=" << chunk_vertices_last
                << " faces=" << chunk_faces_last;
            const io::u32 ping_ms = net_ping_ms.load();
            const io::u32 tps_x100 = net_tps_avg_x100.load();
            if (net_state.load() == 2u) {
                hud << "\nPing: " << ping_ms << " ms";
                hud << "\nServer TPS(avg20): " << (tps_x100 / 100u) << ".";
                const io::u32 frac = tps_x100 % 100u;
                if (frac < 10u) hud << "0";
                hud << frac;
            } else {
                hud << "\nPing: n/a";
                hud << "\nServer TPS(avg20): n/a";
            }
            hud << "\nFPS(avg): " << io::to_u32(frame.dt_history.avg_fps())
                << "  VSync: " << isVSync()
                << "\nWireframe: " << (frame.wireframe_mode ? "true" : "false");
            ge::region::RegionId rid = 0ull;
            io::u16 rm = ge::region::VALUE_MAX;
            io::u16 ri = 0u;
            io::u16 rd = 0u;
            io::u8 rband = 0u;
            io::u8 ncount = 0u;
            io::u8 vcount = 0u;
            io::u32 rsync = 0u;
            region_state_lock.lock();
            rid = region_current_id;
            rm = region_current_mana;
            ri = region_current_instability;
            rd = region_current_decay;
            rband = region_current_bands;
            ncount = region_neighbor_count;
            vcount = region_vertical_neighbor_count;
            rsync = region_last_sync_ms32;
            region_state_lock.unlock();
            const ge::region::Coord rc = ge::region::unpack_region_id(rid);
            const io::i32 wx = floor_to_i32(camera.position[0]);
            const io::i32 wz = floor_to_i32(camera.position[2]);
            const io::i32 approx_border = static_cast<io::i32>(ge::region::border_distance_from_world_xz(wx, wz) + 0.5f);
            const io::u32 now_ms32 = static_cast<io::u32>(io::monotonic_ms() & 0xFFFFFFFFull);
            const io::u32 age_ms = now_ms32 - rsync;
            hud << "\nRegion id: " << ge::net::region_id_hi(rid) << ":" << ge::net::region_id_lo(rid)
                << " cell=(" << rc.cell_x << "," << rc.band_y << "," << rc.cell_z << ")";
            hud << "\nRegion val(m/i/d): " << static_cast<io::u32>(rm) << "/" << static_cast<io::u32>(ri) << "/" << static_cast<io::u32>(rd)
                << " bands=" << static_cast<io::u32>(rband);
            hud << "\nRegion neighbors: " << static_cast<io::u32>(ncount) << " (vertical " << static_cast<io::u32>(vcount) << ")"
                << " sync_age=" << age_ms << "ms";
            hud << "\nRegion border distance: " << approx_border << " blocks";
            hud << "\nF2 developer HUD  F10 player HUD";

            hi::TextDraw td = ge::ui::TextRegular(world_atlas, hud.view());
            td.dock = hi::TextDock::TopL;
            td.x = 12.f;
            td.y = 12.f;
            td.scale = 0.74f;
            td.line_height = 0.8f;
            td.style.r = 0.82f;
            td.style.g = 0.90f;
            td.style.b = 1.00f;
            td.style.a = 0.88f;
            DrawText(td);
        }

        if (show_player_hud && chat_open) {
            hi::TextDraw tip = ge::ui::TextRegular(world_atlas, "Chat is active: Enter send, Tab autocomplete, Esc cancel");
            tip.dock = hi::TextDock::BottomC;
            tip.y = -18.f;
            tip.scale = 0.8f;
            DrawText(tip);
        } else if (show_player_hud && player_dead) {
            hi::TextDraw tip = ge::ui::TextRegular(world_atlas, "You died. Respawning...");
            tip.dock = hi::TextDock::BottomC;
            tip.y = -18.f;
            tip.scale = 0.9f;
            DrawText(tip);
        } else if (show_player_hud && isCursorVisible()) {
            hi::TextDraw tip = ge::ui::TextRegular(world_atlas, "Cursor is visible: camera is paused");
            tip.dock = hi::TextDock::RightC;
            tip.x = 0.f;
            tip.y = 0.f;
            tip.scale = 0.8f;
            DrawText(tip);
        }

        const auto draw_slot_widget = [&](const hi::UiRect& rect,
                                          const ge::item::Stack& stack,
                                          ge::item::SlotRegion region,
                                          io::u8 slot_index,
                                          io::u32 slot_number,
                                          bool selected,
                                          bool show_slot_number,
                                          bool interactive,
                                          bool spell_rainbow_tab) noexcept -> hi::PanelState {
            hi::PanelButtonDraw slot_btn = ge::ui::SlotButton(selected, false, is_dark_theme);
            ge::ui::TintSlotButtonForStack(slot_btn, stack, is_dark_theme);
            if (spell_rainbow_tab && !ge::item::is_empty(stack))
                ge::ui::TintSlotButtonSpellRainbow(slot_btn, frame.scene_time);
            if (region == ge::item::SlotRegion::Trash) {
                slot_btn.fill_normal = ge::ui::Color(0.42f, 0.07f, 0.10f, is_dark_theme ? 0.36f : 0.58f);
                slot_btn.fill_hover = ge::ui::Color(0.46f, 0.10f, 0.14f, is_dark_theme ? 0.42f : 0.66f);
                slot_btn.fill_active = ge::ui::Color(0.52f, 0.12f, 0.16f, is_dark_theme ? 0.48f : 0.72f);
                slot_btn.border_normal = ge::ui::Color(0.78f, 0.24f, 0.24f, is_dark_theme ? 0.44f : 0.58f);
                slot_btn.border_hover = ge::ui::Color(0.86f, 0.30f, 0.30f, is_dark_theme ? 0.52f : 0.66f);
                slot_btn.border_active = ge::ui::Color(0.94f, 0.36f, 0.36f, is_dark_theme ? 0.58f : 0.74f);
            }
            slot_btn.dock = hi::TextDock::TopL;
            slot_btn.x = rect.x;
            slot_btn.y = rect.y;
            slot_btn.w = rect.w;
            slot_btn.h = rect.h;
            hi::PanelState state = PanelButton(slot_btn);
            if (!interactive) {
                state.clicked = false;
                state.held = false;
            }
            if (interactive && inventory_open && state.hovered) {
                inventory_hover_valid = true;
                inventory_hover_region = region;
                inventory_hover_index = slot_index;
            }

            const float icon_pad = 10.f;
            const float inner_w = rect.w - icon_pad * 2.f;
            const float inner_h = rect.h - icon_pad * 2.f;
            const float icon_extent = (inner_w < inner_h) ? inner_w : inner_h;
            float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
            if (gui_texture_atlas >= 0 && ItemIconUv(stack, u0, v0, u1, v1) && icon_extent > 4.f) {
                hi::ImageDraw image{};
                image.atlas = gui_texture_atlas;
                image.dock = hi::TextDock::TopL;
                image.x = rect.x + 0.5f * (rect.w - icon_extent);
                image.y = rect.y + 0.5f * (rect.h - icon_extent);
                image.w = icon_extent;
                image.h = icon_extent;
                image.u0 = u0;
                image.v0 = v0;
                image.u1 = u1;
                image.v1 = v1;
                image.tint = ge::ui::Color(1.f, 1.f, 1.f, 1.f);
                if (ge::item::decays(stack.id) && ge::item::freshness_band(stack) == ge::item::FreshnessBand::Rotten)
                    image.tint = ge::ui::Color(0.72f, 0.82f, 0.56f, 1.f);
                DrawImage(image);
            }

            if (show_slot_number) {
                io::StackOut<8> slot_text{};
                slot_text << slot_number;
                hi::TextDraw slot_td = ge::ui::TextRegular(world_atlas, slot_text.view());
                slot_td.dock = hi::TextDock::TopL;
                slot_td.x = rect.x + 8.f;
                slot_td.y = rect.y + 6.f;
                slot_td.scale = 0.56f;
                slot_td.style.a = 0.82f;
                DrawText(slot_td);
            }

            if (!ge::item::is_empty(stack)) {
                io::StackOut<16> count_text{};
                count_text << stack.count;
                hi::TextDraw count_td = ge::ui::TextRegular(world_atlas, count_text.view());
                count_td.dock = hi::TextDock::BottomR;
                count_td.x = rect.x1() - static_cast<float>(width()) - 7.f;
                count_td.y = rect.y1() - static_cast<float>(height()) - 5.f;
                count_td.scale = 0.72f;
                count_td.style.outline = true;
                count_td.style.or_ = 0.02f;
                count_td.style.og_ = 0.02f;
                count_td.style.ob_ = 0.04f;
                count_td.style.oa_ = 0.95f;
                DrawText(count_td);

                if (ge::item::decays(stack.id) && ge::item::freshness_band(stack) == ge::item::FreshnessBand::Rotten) {
                    hi::TextDraw rotten_td = ge::ui::TextRegular(world_atlas, "ROT");
                    rotten_td.dock = hi::TextDock::TopR;
                    rotten_td.x = rect.x1() - static_cast<float>(width()) - 7.f;
                    rotten_td.y = rect.y + 6.f;
                    rotten_td.scale = 0.48f;
                    rotten_td.style.r = 0.86f;
                    rotten_td.style.g = 0.94f;
                    rotten_td.style.b = 0.54f;
                    rotten_td.style.a = 0.92f;
                    DrawText(rotten_td);
                }
            }

            return state;
        };

        const float slot_size = inventory_open ? 68.f : 72.f;
        const float hotbar_gap = 10.f;
        const float hotbar_w = slot_size * static_cast<float>(ge::item::HOTBAR_SLOT_COUNT) +
                               hotbar_gap * static_cast<float>(ge::item::HOTBAR_SLOT_COUNT - 1u) + 28.f;
        const float hotbar_h = slot_size + 24.f;
        const float hotbar_y = (inventory_open ? -110.f : -56.f) + 50.f;
        inventory_hover_valid = false;
        ward_config_hover_valid = false;

        hi::PanelDraw hotbar_panel = ge::ui::PanelCard(is_dark_theme);
        hotbar_panel.dock = hi::TextDock::BottomC;
        hotbar_panel.y = hotbar_y;
        hotbar_panel.w = hotbar_w;
        hotbar_panel.h = hotbar_h;
        if (is_dark_theme) {
            hotbar_panel.fill = ge::ui::Color(0.24f, 0.24f, 0.25f, 0.26f);
            hotbar_panel.border = ge::ui::Color(0.01f, 0.03f, 0.05f, 0.14f);
        } else {
            hotbar_panel.fill = ge::ui::Color(0.35f, 0.30f, 0.20f, 0.72f);
            hotbar_panel.border = ge::ui::Color(0.50f, 0.58f, 0.62f, 0.24f);
        }
        DrawPanel(hotbar_panel);

        hi::FlexLayout hotbar_layout{};
        hotbar_layout.dock = hi::TextDock::BottomC;
        hotbar_layout.y = hotbar_y;
        hotbar_layout.w = hotbar_w;
        hotbar_layout.h = hotbar_h;
        hotbar_layout.axis = hi::LayoutAxis::Row;
        hotbar_layout.gap = hotbar_gap;
        hotbar_layout.padding.left = 14.f;
        hotbar_layout.padding.top = 12.f;
        hotbar_layout.padding.right = 14.f;
        hotbar_layout.padding.bottom = 12.f;
        hi::FlexState hotbar = BeginFlex(hotbar_layout);
        for (io::u32 i = 0u; i < ge::item::HOTBAR_SLOT_COUNT; ++i) {
            const hi::UiRect cell = NextFlexRect(hotbar, slot_size, slot_size);
            const hi::PanelState state = draw_slot_widget(cell, inventory_state.hotbar[i],
                                                          ge::item::SlotRegion::Hotbar, static_cast<io::u8>(i),
                                                          i + 1u,
                                                          i == inventory_state.selected_hotbar,
                                                          true, true, false);
            if (!inventory_open && state.clicked) {
                SelectQuickSlot(i);
            }
        }

        const io::u64 now_ms = io::monotonic_ms();
        const ge::item::Stack& selected_stack = SelectedHotbarStack();
        if (!ge::item::is_empty(selected_stack) && hotbar_hint_until_ms > now_ms) {
            const io::u32 remain_ms = static_cast<io::u32>(hotbar_hint_until_ms - now_ms);
            float alpha = static_cast<float>(remain_ms) / static_cast<float>(HOTBAR_HINT_DURATION_MS);
            if (alpha < 0.f) alpha = 0.f;
            if (alpha > 1.f) alpha = 1.f;
            alpha *= alpha;

            hi::TextDraw name_td = ge::ui::TextRegular(world_atlas, ge::item::name(selected_stack.id));
            name_td.dock = hi::TextDock::BottomC;
            name_td.y = hotbar_y - (inventory_open ? 124.f : 120.f);
            name_td.scale = 0.86f;
            ge::ui::ApplyTextColor(name_td, ge::ui::ItemAccentColor(selected_stack));
            name_td.style.a *= alpha;
            DrawText(name_td);
        }

        if (show_player_hud && remote_players &&
            (screen == ScreenState::InGame || screen == ScreenState::InGameDead)) {
            RemotePlayerVisual snapshots[REMOTE_PLAYER_CAP]{};
            io::u32 snapshot_count = 0u;
            remote_players_lock.lock();
            for (io::u32 i = 0u; i < REMOTE_PLAYER_CAP; ++i) {
                if (!remote_players[i].active) continue;
                if (snapshot_count >= REMOTE_PLAYER_CAP) break;
                snapshots[snapshot_count++] = remote_players[i];
            }
            remote_players_lock.unlock();

            const float h = height() > 0 ? static_cast<float>(height()) : 1.f;
            const float aspect = static_cast<float>(width()) / h;
            const float rx = static_cast<float>(render_distance_chunks * ge::voxel::CHUNK_SIZE);
            const float ry = static_cast<float>(WorldYRadiusChunks() * ge::voxel::CHUNK_SIZE);
            const float rz = static_cast<float>(render_distance_chunks * ge::voxel::CHUNK_SIZE);
            float far_plane = lm::sqrtf(rx * rx + ry * ry + rz * rz) + 96.f;
            if (far_plane < 420.f) far_plane = 420.f;
            const lm::mat4 view = camera.view_matrix();
            const lm::mat4 proj = camera.projection_matrix(aspect, 0.1f, far_plane);
            const lm::mat4 vp = proj * view;
            const auto mul_clip = [](const lm::mat4& m, float x, float y, float z, float& ox, float& oy, float& oz, float& ow) noexcept {
                ox = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
                oy = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
                oz = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2];
                ow = m[0][3] * x + m[1][3] * y + m[2][3] * z + m[3][3];
            };

            for (io::u32 i = 0u; i < snapshot_count; ++i) {
                io::char_view roster_name{};
                if (!RosterNameByServerIndex(snapshots[i].server_index, roster_name) || roster_name.empty())
                    continue;
                float cx = 0.f, cy = 0.f, cz = 0.f, cw = 0.f;
                mul_clip(vp, snapshots[i].x, snapshots[i].y + 0.45f, snapshots[i].z, cx, cy, cz, cw);
                if (cw <= 0.0001f) continue;
                const float inv_w = 1.f / cw;
                const float ndc_x = cx * inv_w;
                const float ndc_y = cy * inv_w;
                if (ndc_x < -1.10f || ndc_x > 1.10f || ndc_y < -1.10f || ndc_y > 1.10f)
                    continue;
                const float sx = (ndc_x * 0.5f + 0.5f) * static_cast<float>(width());
                const float sy = (1.f - (ndc_y * 0.5f + 0.5f)) * static_cast<float>(height());
                hi::TextDraw tag_td = ge::ui::TextRegular(world_atlas, roster_name);
                tag_td.dock = hi::TextDock::TopC;
                tag_td.x = sx - static_cast<float>(width()) * 0.5f;
                tag_td.y = sy - 18.f;
                tag_td.scale = 0.62f;
                tag_td.style.r = 0.90f;
                tag_td.style.g = 0.95f;
                tag_td.style.b = 1.f;
                tag_td.style.a = 0.86f;
                DrawText(tag_td);
            }
        }

        if ((screen == ScreenState::InGame || screen == ScreenState::InGameDead) &&
            !chat_open && hi::Key_t::isPressed(hi::Key::Tab) && player_roster_names) {
            io::u8 roster_name_len[ge::net::PLAYER_ROSTER_CLIENT_CAP]{};
            char roster_name[ge::net::PLAYER_ROSTER_CLIENT_CAP][ge::net::PLAYER_NICK_BYTES + 1]{};
            ge::net::SignalQuality roster_quality[ge::net::PLAYER_ROSTER_CLIENT_CAP]{};
            io::u16 roster_server_index[ge::net::PLAYER_ROSTER_CLIENT_CAP]{};
            io::u32 roster_count = 0u;
            player_roster_lock.lock();
            const auto same_name = [&](io::u32 a, const char* b_name, io::u8 b_len) noexcept {
                if (roster_name_len[a] != b_len) return false;
                for (io::u32 n = 0u; n < b_len; ++n)
                    if (roster_name[a][n] != b_name[n])
                        return false;
                return true;
            };
            const auto already_added = [&](io::u16 server_index, const char* name_ptr, io::u8 name_len) noexcept {
                for (io::u32 e = 0u; e < roster_count; ++e) {
                    if (server_index != 0xFFFFu && roster_server_index[e] != 0xFFFFu) {
                        if (roster_server_index[e] == server_index)
                            return true;
                    } else if (same_name(e, name_ptr, name_len)) {
                        return true;
                    }
                }
                return false;
            };
            for (io::u32 i = 0u; i < player_roster_count && i < ge::net::PLAYER_ROSTER_CLIENT_CAP; ++i) {
                const io::u32 slot = (player_roster_head + i) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
                const io::u8 len = player_roster_name_len[slot];
                if (len == 0u) continue;
                const char* src = player_roster_names + static_cast<io::usize>(slot) * ge::net::PLAYER_NICK_BYTES;
                const io::u16 server_index = player_roster_server_index[slot];
                if (already_added(server_index, src, len))
                    continue;
                roster_name_len[roster_count] = len;
                for (io::u32 n = 0u; n < ge::net::PLAYER_NICK_BYTES; ++n)
                    roster_name[roster_count][n] = (n < len) ? src[n] : '\0';
                roster_name[roster_count][len] = '\0';
                roster_server_index[roster_count] = server_index;
                roster_quality[roster_count] = PlayerRosterSignalAt(slot);
                ++roster_count;
            }
            player_roster_lock.unlock();

            const io::u32 max_rows = 12u;
            const io::u32 rows = (roster_count == 0u) ? 1u : ((roster_count < max_rows) ? roster_count : max_rows);
            const io::u32 cols = (roster_count + rows - 1u) / rows;
            const float cell_w = 220.f;
            const float cell_h = 22.f;
            const float panel_w = 24.f + static_cast<float>(cols) * cell_w;
            const float panel_h = 38.f + static_cast<float>(rows) * cell_h + 12.f;

            hi::PanelDraw roster_panel = ge::ui::PanelCard(is_dark_theme);
            roster_panel.dock = hi::TextDock::TopC;
            roster_panel.y = 10.f;
            roster_panel.w = panel_w;
            roster_panel.h = panel_h;
            roster_panel.fill = is_dark_theme
                ? ge::ui::Color(0.04f, 0.05f, 0.06f, 0.82f)
                : ge::ui::Color(0.20f, 0.19f, 0.17f, 0.86f);
            roster_panel.border = is_dark_theme
                ? ge::ui::Color(0.42f, 0.48f, 0.52f, 0.28f)
                : ge::ui::Color(0.56f, 0.62f, 0.66f, 0.30f);
            DrawPanel(roster_panel);

            io::StackOut<96> roster_title{};
            roster_title << "Players online: " << roster_count;
            hi::TextDraw title_td = ge::ui::TextRegular(world_atlas, roster_title.view());
            title_td.dock = hi::TextDock::TopC;
            title_td.y = 20.f;
            title_td.scale = 0.78f;
            DrawText(title_td);

            const float panel_left = 0.5f * (static_cast<float>(width()) - panel_w);
            const float row_y0 = 44.f;
            if (roster_count == 0u) {
                hi::TextDraw empty_td = ge::ui::TextRegular(world_atlas, "No players");
                empty_td.dock = hi::TextDock::TopC;
                empty_td.y = row_y0 + 8.f;
                empty_td.scale = 0.72f;
                empty_td.style.a = 0.74f;
                DrawText(empty_td);
            } else {
                for (io::u32 i = 0u; i < roster_count; ++i) {
                    const io::u32 col = i / rows;
                    const io::u32 row = i % rows;
                    const char* name_ptr = roster_name[i];
                    const io::u8 name_len = roster_name_len[i];
                    const ge::net::SignalQuality quality = roster_quality[i];
                    io::char_view quality_text = "Bad";
                    switch (quality) {
                    case ge::net::SignalQuality::Excellent: quality_text = "Excellent"; break;
                    case ge::net::SignalQuality::Good: quality_text = "Good"; break;
                    case ge::net::SignalQuality::Okay: quality_text = "Okay"; break;
                    default: break;
                    }
                    io::StackOut<96> row_text{};
                    row_text << io::char_view{ name_ptr, name_len } << "  " << quality_text;
                    hi::TextDraw row_td = ge::ui::TextRegular(world_atlas, row_text.view());
                    row_td.dock = hi::TextDock::TopL;
                    row_td.x = panel_left + 12.f + static_cast<float>(col) * cell_w;
                    row_td.y = row_y0 + static_cast<float>(row) * cell_h;
                    row_td.scale = 0.68f;
                    switch (quality) {
                    case ge::net::SignalQuality::Excellent:
                        row_td.style.r = 0.52f; row_td.style.g = 0.96f; row_td.style.b = 0.58f; break;
                    case ge::net::SignalQuality::Good:
                        row_td.style.r = 0.72f; row_td.style.g = 0.92f; row_td.style.b = 1.f; break;
                    case ge::net::SignalQuality::Okay:
                        row_td.style.r = 1.f; row_td.style.g = 0.86f; row_td.style.b = 0.38f; break;
                    default:
                        row_td.style.r = 0.98f; row_td.style.g = 0.46f; row_td.style.b = 0.40f; break;
                    }
                    DrawText(row_td);
                }
            }
        }

        if (inventory_open) {
            SyncWardConfigsFromInventory();
            const float inv_panel_w = 860.f;
            const float inv_panel_h = 388.f;
            const float inv_panel_y = 58.f;

            hi::PanelDraw inventory_panel = ge::ui::PanelCard(is_dark_theme);
            inventory_panel.dock = hi::TextDock::TopC;
            inventory_panel.y = inv_panel_y;
            inventory_panel.w = inv_panel_w;
            inventory_panel.h = inv_panel_h;
            if (is_dark_theme) {
                inventory_panel.fill = ge::ui::Color(0.05f, 0.05f, 0.06f, 0.54f);
                inventory_panel.border = ge::ui::Color(0.42f, 0.50f, 0.54f, 0.16f);
            } else {
                inventory_panel.fill = ge::ui::Color(0.38f, 0.33f, 0.22f, 0.74f);
                inventory_panel.border = ge::ui::Color(0.52f, 0.60f, 0.64f, 0.28f);
            }
            DrawPanel(inventory_panel);

            hi::TextDraw title = ge::ui::TextHeader(world_atlas, "Inventory");
            title.dock = hi::TextDock::TopC;
            title.y = inv_panel_y + 16.f;
            title.scale = 1.2f;
            DrawText(title);

            const struct {
                InventoryUiTab tab;
                const char* name;
            } tabs[] = {
                { InventoryUiTab::General, "General" },
                { InventoryUiTab::Blocks, "Blocks" },
                { InventoryUiTab::Spells, "Spells" }
            };

            const auto draw_tab_button = [&](const hi::UiRect& tab_rect,
                                             InventoryUiTab tab_id,
                                             io::char_view label) noexcept {
                hi::PanelButtonDraw tab_btn = ge::ui::SlotButton(inventory_tab == tab_id, false, is_dark_theme);
                tab_btn.dock = hi::TextDock::TopL;
                tab_btn.x = tab_rect.x;
                tab_btn.y = tab_rect.y;
                tab_btn.w = tab_rect.w;
                tab_btn.h = tab_rect.h;
                tab_btn.style.border_radius = 12.f;
                const hi::PanelState tab_state = PanelButton(tab_btn);
                if (tab_state.clicked)
                    inventory_tab = tab_id;

                hi::TextDraw tab_td = ge::ui::TextRegular(world_atlas, label);
                tab_td.dock = hi::TextDock::TopC;
                tab_td.x = (tab_rect.x + tab_rect.w * 0.5f) - static_cast<float>(width()) * 0.5f;
                tab_td.y = tab_rect.y + 11.f;
                tab_td.scale = 0.84f;
                if (inventory_tab == tab_id) {
                    tab_td.style.r = 0.96f;
                    tab_td.style.g = 0.90f;
                    tab_td.style.b = 0.74f;
                    tab_td.style.a = 0.92f;
                } else {
                    tab_td.style.r = 0.84f;
                    tab_td.style.g = 0.85f;
                    tab_td.style.b = 0.89f;
                    tab_td.style.a = 0.78f;
                }
                DrawText(tab_td);
            };

            hi::FlexLayout tabs_layout{};
            tabs_layout.dock = hi::TextDock::TopC;
            tabs_layout.y = inv_panel_y + 66.f;
            tabs_layout.w = 836.f;
            tabs_layout.h = 46.f;
            tabs_layout.axis = hi::LayoutAxis::Row;
            tabs_layout.gap = 12.f;
            hi::FlexState tabs_flex = BeginFlex(tabs_layout);
            for (io::u32 tab_i = 0u; tab_i < 3u; ++tab_i) {
                const hi::UiRect tab_rect = NextFlexRect(tabs_flex, 270.f, 46.f);
                draw_tab_button(tab_rect, tabs[tab_i].tab, tabs[tab_i].name);
            }

            bool has_any_ward = false;
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) {
                if (HasWardAtIndex(i)) {
                    has_any_ward = true;
                    break;
                }
            }
            if (!has_any_ward)
                ward_config_open = false;

            if (inventory_tab == InventoryUiTab::General) {
                const float inv_left = 0.5f * (static_cast<float>(width()) - inv_panel_w);
                hi::UiRect cfg_rect{};
                cfg_rect.w = 258.f;
                cfg_rect.h = 46.f;
                cfg_rect.x = inv_left + inv_panel_w - cfg_rect.w - 18.f;
                cfg_rect.y = inv_panel_y + 118.f;
                hi::PanelButtonDraw config_btn = ge::ui::SlotButton(ward_config_open, false, is_dark_theme);
                config_btn.dock = hi::TextDock::TopL;
                config_btn.x = cfg_rect.x;
                config_btn.y = cfg_rect.y;
                config_btn.w = cfg_rect.w;
                config_btn.h = cfg_rect.h;
                config_btn.style.border_radius = 12.f;
                if (!has_any_ward) {
                    config_btn.fill_normal.a *= 0.55f;
                    config_btn.fill_hover.a *= 0.55f;
                    config_btn.fill_active.a *= 0.55f;
                }
                const hi::PanelState cfg_btn_state = PanelButton(config_btn);
                if (cfg_btn_state.clicked && has_any_ward)
                    ward_config_open = !ward_config_open;

                hi::TextDraw cfg_td = ge::ui::TextRegular(world_atlas, "Ward Configuration");
                cfg_td.dock = hi::TextDock::TopL;
                cfg_td.x = cfg_rect.x + 24.f;
                cfg_td.y = cfg_rect.y + 11.f;
                cfg_td.scale = 0.74f;
                if (!has_any_ward) {
                    cfg_td.style.r = 0.62f;
                    cfg_td.style.g = 0.66f;
                    cfg_td.style.b = 0.72f;
                    cfg_td.style.a = 0.58f;
                } else if (ward_config_open) {
                    cfg_td.style.r = 0.96f;
                    cfg_td.style.g = 0.90f;
                    cfg_td.style.b = 0.74f;
                    cfg_td.style.a = 0.92f;
                } else {
                    cfg_td.style.r = 0.84f;
                    cfg_td.style.g = 0.85f;
                    cfg_td.style.b = 0.89f;
                    cfg_td.style.a = 0.78f;
                }
                DrawText(cfg_td);
            } else {
                ward_config_hover_valid = false;
            }

            hi::GridLayout inventory_grid_layout{};
            inventory_grid_layout.dock = hi::TextDock::TopC;
            inventory_grid_layout.y = inv_panel_y + 178.f;
            inventory_grid_layout.w = 736.f;
            inventory_grid_layout.h = 170.f;
            inventory_grid_layout.columns = 9u;
            inventory_grid_layout.gap_x = 10.f;
            inventory_grid_layout.gap_y = 10.f;
            inventory_grid_layout.cell_h = 50.f;
            hi::GridState inventory_grid = BeginGrid(inventory_grid_layout);
            const ge::item::SlotRegion tab_region = InventoryRegionForTab(inventory_tab);
            const bool spell_rainbow_tab = (inventory_tab == InventoryUiTab::Spells);
            for (io::u32 row = 0u; row < 3u; ++row) {
                for (io::u32 col = 0u; col < 9u; ++col) {
                    const io::u32 index = row * 9u + col;
                    const ge::item::Stack* shown_ptr = ge::item::slot_ptr(inventory_state, tab_region, static_cast<io::u8>(index));
                    const ge::item::Stack shown = shown_ptr ? *shown_ptr : ge::item::Stack{};
                    const hi::UiRect cell = NextGridRect(inventory_grid);
                    (void)draw_slot_widget(cell, shown,
                                           tab_region, static_cast<io::u8>(index),
                                           index + 1u,
                                           false, false, true, spell_rainbow_tab);
                }
            }

            const float inv_left = 0.5f * (static_cast<float>(width()) - inv_panel_w);
            const float trash_size = 50.f;
            hi::UiRect trash_rect{};
            trash_rect.x = inv_left + inv_panel_w - trash_size - 26.f;
            trash_rect.y = inventory_grid_layout.y + inventory_grid_layout.h - trash_size - 2.f;
            trash_rect.w = trash_size;
            trash_rect.h = trash_size;
            (void)draw_slot_widget(trash_rect, inventory_state.trash,
                                   ge::item::SlotRegion::Trash, 0u,
                                   0u, false, false, true, false);

            hi::TextDraw trash_td = ge::ui::TextRegular(world_atlas, "Trash");
            trash_td.dock = hi::TextDock::TopL;
            trash_td.x = trash_rect.x + 6.f;
            trash_td.y = trash_rect.y - 18.f;
            trash_td.scale = 0.62f;
            trash_td.style.a = 0.76f;
            DrawText(trash_td);

            io::u32 free_slots = 0u;
            for (io::u32 i = 0u; i < ge::item::HOTBAR_SLOT_COUNT; ++i)
                if (ge::item::is_empty(inventory_state.hotbar[i])) ++free_slots;
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                if (ge::item::is_empty(inventory_state.blocks[i])) ++free_slots;
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i)
                if (ge::item::is_empty(inventory_state.spells[i])) ++free_slots;
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) {
                const ge::item::Stack* general_slot =
                    ge::item::slot_ptr(inventory_state, ge::item::SlotRegion::General, static_cast<io::u8>(i));
                if (!general_slot || ge::item::is_empty(*general_slot))
                    ++free_slots;
            }
            static constexpr io::u32 inventory_region_count = 3u;
            static constexpr io::u32 total_slots =
                inventory_region_count * ge::item::INVENTORY_SLOT_COUNT + ge::item::HOTBAR_SLOT_COUNT;

            io::StackOut<160> inventory_hint_text{};
            inventory_hint_text << "Free slots: " << free_slots
                                << " avail. / " << inventory_region_count
                                << " inventories * 27 + 9 Quick slots";
            hi::TextDraw inventory_hint = ge::ui::TextRegular(world_atlas, inventory_hint_text.view());
            inventory_hint.dock = hi::TextDock::TopC;
            inventory_hint.x = 0.f;
            inventory_hint.y = inv_panel_y + inv_panel_h - 28.f;
            inventory_hint.scale = 0.70f;
            inventory_hint.style.a = 0.76f;
            DrawText(inventory_hint);

            if (inventory_tab == InventoryUiTab::General && ward_config_open && has_any_ward) {
                if (!HasWardAtIndex(ward_config_selected_index)) {
                    for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) {
                        if (!HasWardAtIndex(i)) continue;
                        ward_config_selected_index = static_cast<io::u8>(i);
                        break;
                    }
                }

                if (HasWardAtIndex(ward_config_selected_index)) {
                    const float inv_left = 0.5f * (static_cast<float>(width()) - inv_panel_w);
                    const float cfg_left = inv_left;
                    const float cfg_top = inv_panel_y + inv_panel_h + 14.f;
                    const float cfg_panel_w = inv_panel_w;
                    const float cfg_panel_h = 346.f;

                    hi::PanelDraw cfg_panel = ge::ui::PanelCard(is_dark_theme);
                    cfg_panel.dock = hi::TextDock::TopL;
                    cfg_panel.x = cfg_left;
                    cfg_panel.y = cfg_top;
                    cfg_panel.w = cfg_panel_w;
                    cfg_panel.h = cfg_panel_h;
                    if (is_dark_theme) {
                        cfg_panel.fill = ge::ui::Color(0.03f, 0.04f, 0.05f, 0.56f);
                        cfg_panel.border = ge::ui::Color(0.38f, 0.46f, 0.50f, 0.16f);
                    } else {
                        cfg_panel.fill = ge::ui::Color(0.30f, 0.27f, 0.19f, 0.74f);
                        cfg_panel.border = ge::ui::Color(0.54f, 0.60f, 0.64f, 0.24f);
                    }
                    DrawPanel(cfg_panel);

                    const WardConfigState* cfg_ptr = SelectedWardConfig();
                    const WardConfigState empty_cfg{};
                    const WardConfigState& cfg = (cfg_ptr && cfg_ptr->valid) ? *cfg_ptr : empty_cfg;
                    const bool cfg_ready = (cfg_ptr && cfg_ptr->valid);
                    const ge::item::Stack& ward_stack = inventory_state.spelling_wards[ward_config_selected_index];

                    hi::TextDraw cfg_title = ge::ui::TextHeader(world_atlas, "Ward Configuration");
                    cfg_title.dock = hi::TextDock::TopL;
                    cfg_title.x = cfg_left + cfg_panel_w - 304.f;
                    cfg_title.y = cfg_top + 14.f;
                    cfg_title.scale = 0.90f;
                    DrawText(cfg_title);

                    hi::TextDraw ward_name = ge::ui::TextRegular(world_atlas, ge::item::name(ward_stack.id));
                    ward_name.dock = hi::TextDock::TopL;
                    ward_name.x = cfg_left + 18.f;
                    ward_name.y = cfg_top + 44.f;
                    ward_name.scale = 0.84f;
                    ge::ui::ApplyTextColor(ward_name, ge::ui::ItemAccentColor(ward_stack));
                    DrawText(ward_name);

                    hi::GridLayout ward_grid_layout{};
                    ward_grid_layout.dock = hi::TextDock::TopL;
                    ward_grid_layout.x = cfg_left + 18.f;
                    ward_grid_layout.y = cfg_top + 64.f;
                    ward_grid_layout.w = cfg_panel_w - 36.f;
                    ward_grid_layout.h = 170.f;
                    ward_grid_layout.columns = 9u;
                    ward_grid_layout.gap_x = 10.f;
                    ward_grid_layout.gap_y = 10.f;
                    ward_grid_layout.cell_h = 50.f;
                    hi::GridState ward_grid = BeginGrid(ward_grid_layout);

                    const float stat_y0 = ward_grid_layout.y + ward_grid_layout.h + 16.f;
                    const float stat_x_l = cfg_left + 18.f;
                    const float stat_x_r = cfg_left + 430.f;

                    io::StackOut<160> stat0{};
                    stat0 << "Speed: " << cfg.stat_speed;
                    hi::TextDraw stat0_td = ge::ui::TextRegular(world_atlas, stat0.view());
                    stat0_td.dock = hi::TextDock::TopL;
                    stat0_td.x = stat_x_l;
                    stat0_td.y = stat_y0;
                    stat0_td.scale = 0.72f;
                    stat0_td.style.a = 0.86f;
                    DrawText(stat0_td);

                    io::StackOut<160> stat1{};
                    stat1 << "Delay Cast: " << cfg.stat_delay_cast;
                    hi::TextDraw stat1_td = ge::ui::TextRegular(world_atlas, stat1.view());
                    stat1_td.dock = hi::TextDock::TopL;
                    stat1_td.x = stat_x_l;
                    stat1_td.y = stat_y0 + 22.f;
                    stat1_td.scale = 0.72f;
                    stat1_td.style.a = 0.86f;
                    DrawText(stat1_td);

                    io::StackOut<160> stat2{};
                    stat2 << "Delay Reload: " << cfg.stat_delay_reload;
                    hi::TextDraw stat2_td = ge::ui::TextRegular(world_atlas, stat2.view());
                    stat2_td.dock = hi::TextDock::TopL;
                    stat2_td.x = stat_x_r;
                    stat2_td.y = stat_y0;
                    stat2_td.scale = 0.72f;
                    stat2_td.style.a = 0.86f;
                    DrawText(stat2_td);

                    io::StackOut<160> stat3{};
                    stat3 << "Spread: " << cfg.stat_spread;
                    hi::TextDraw stat3_td = ge::ui::TextRegular(world_atlas, stat3.view());
                    stat3_td.dock = hi::TextDock::TopL;
                    stat3_td.x = stat_x_r;
                    stat3_td.y = stat_y0 + 22.f;
                    stat3_td.scale = 0.72f;
                    stat3_td.style.a = 0.86f;
                    DrawText(stat3_td);

                    io::StackOut<160> stat4{};
                    stat4 << "Slots available (max 27, now " << static_cast<io::u32>(cfg.slots_available) << ")";
                    hi::TextDraw stat4_td = ge::ui::TextRegular(world_atlas, stat4.view());
                    stat4_td.dock = hi::TextDock::TopL;
                    stat4_td.x = stat_x_l;
                    stat4_td.y = stat_y0 + 52.f;
                    stat4_td.scale = 0.68f;
                    stat4_td.style.a = 0.84f;
                    DrawText(stat4_td);

                    hi::TextDraw cfg_hint = ge::ui::TextRegular(world_atlas,
                        cfg_ready ? "Click ward slots to insert/remove spells" : "Syncing ward config from server...");
                    cfg_hint.dock = hi::TextDock::TopL;
                    cfg_hint.x = stat_x_l;
                    cfg_hint.y = stat_y0 + 80.f;
                    cfg_hint.scale = 0.64f;
                    cfg_hint.style.a = 0.72f;
                    DrawText(cfg_hint);

                    for (io::u32 i = 0u; i < WARD_CONFIG_SLOT_MAX; ++i) {
                        const bool slot_enabled = cfg_ready && (i < cfg.slots_available);
                        const ge::item::Stack& spell_stack = cfg.spells[i];
                        const hi::UiRect cell = NextGridRect(ward_grid);

                        hi::PanelButtonDraw spell_btn = ge::ui::SlotButton(false, false, is_dark_theme);
                        ge::ui::TintSlotButtonForStack(spell_btn, spell_stack, is_dark_theme);
                        if (!ge::item::is_empty(spell_stack))
                            ge::ui::TintSlotButtonSpellRainbow(spell_btn, frame.scene_time + static_cast<float>(i) * 0.11f);
                        if (!slot_enabled) {
                            spell_btn.fill_normal = ge::ui::Color(0.f, 0.f, 0.f, is_dark_theme ? 0.36f : 0.64f);
                            spell_btn.fill_hover = spell_btn.fill_normal;
                            spell_btn.fill_active = spell_btn.fill_normal;
                            spell_btn.border_normal = ge::ui::Color(0.24f, 0.28f, 0.32f, 0.18f);
                            spell_btn.border_hover = spell_btn.border_normal;
                            spell_btn.border_active = spell_btn.border_normal;
                        }
                        spell_btn.dock = hi::TextDock::TopL;
                        spell_btn.x = cell.x;
                        spell_btn.y = cell.y;
                        spell_btn.w = cell.w;
                        spell_btn.h = cell.h;

                        hi::PanelState spell_state = PanelButton(spell_btn);
                        if (!slot_enabled) {
                            spell_state.hovered = false;
                            spell_state.clicked = false;
                            spell_state.held = false;
                        } else if (spell_state.hovered) {
                            ward_config_hover_valid = true;
                            ward_config_hover_slot = static_cast<io::u8>(i);
                        }

                        const float icon_pad = 10.f;
                        const float inner_w = cell.w - icon_pad * 2.f;
                        const float inner_h = cell.h - icon_pad * 2.f;
                        const float icon_extent = (inner_w < inner_h) ? inner_w : inner_h;
                        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
                        if (slot_enabled && gui_texture_atlas >= 0 && ItemIconUv(spell_stack, u0, v0, u1, v1) && icon_extent > 4.f) {
                            hi::ImageDraw image{};
                            image.atlas = gui_texture_atlas;
                            image.dock = hi::TextDock::TopL;
                            image.x = cell.x + 0.5f * (cell.w - icon_extent);
                            image.y = cell.y + 0.5f * (cell.h - icon_extent);
                            image.w = icon_extent;
                            image.h = icon_extent;
                            image.u0 = u0;
                            image.v0 = v0;
                            image.u1 = u1;
                            image.v1 = v1;
                            image.tint = ge::ui::Color(1.f, 1.f, 1.f, 1.f);
                            DrawImage(image);
                        }

                        io::StackOut<8> slot_no{};
                        slot_no << (i + 1u);
                        hi::TextDraw slot_no_td = ge::ui::TextRegular(world_atlas, slot_no.view());
                        slot_no_td.dock = hi::TextDock::TopL;
                        slot_no_td.x = cell.x + 8.f;
                        slot_no_td.y = cell.y + 6.f;
                        slot_no_td.scale = 0.56f;
                        slot_no_td.style.a = slot_enabled ? 0.74f : 0.32f;
                        DrawText(slot_no_td);

                        if (slot_enabled && !ge::item::is_empty(spell_stack)) {
                            io::StackOut<16> count_text{};
                            count_text << spell_stack.count;
                            hi::TextDraw count_td = ge::ui::TextRegular(world_atlas, count_text.view());
                            count_td.dock = hi::TextDock::BottomR;
                            count_td.x = cell.x1() - static_cast<float>(width()) - 7.f;
                            count_td.y = cell.y1() - static_cast<float>(height()) - 5.f;
                            count_td.scale = 0.72f;
                            count_td.style.outline = true;
                            count_td.style.or_ = 0.02f;
                            count_td.style.og_ = 0.02f;
                            count_td.style.ob_ = 0.04f;
                            count_td.style.oa_ = 0.95f;
                            DrawText(count_td);
                        }
                    }
                }
            }
        }

        if (help_window_open) {
            static constexpr io::u32 per_page = 6u;
            const io::u32 total_cmds = CHAT_COMMAND_SPEC_COUNT;
            const io::u32 total_pages = (total_cmds + per_page - 1u) / per_page;
            if (help_window_page >= total_pages)
                help_window_page = (total_pages == 0u) ? 0u : (total_pages - 1u);

            const float panel_w = 860.f;
            const float panel_h = 330.f;
            const float panel_y = 84.f;
            const float panel_left = 0.5f * (static_cast<float>(width()) - panel_w);
            const float help_content_offset_y = 15.f;

            hi::PanelDraw help_panel = ge::ui::PanelCard(is_dark_theme);
            help_panel.dock = hi::TextDock::TopC;
            help_panel.y = panel_y;
            help_panel.w = panel_w;
            help_panel.h = panel_h;
            help_panel.fill = is_dark_theme
                ? ge::ui::Color(0.03f, 0.04f, 0.05f, 0.86f)
                : ge::ui::Color(0.18f, 0.16f, 0.12f, 0.88f);
            help_panel.border = is_dark_theme
                ? ge::ui::Color(0.40f, 0.48f, 0.52f, 0.30f)
                : ge::ui::Color(0.58f, 0.64f, 0.68f, 0.30f);
            DrawPanel(help_panel);

            hi::TextDraw help_title = ge::ui::TextHeader(world_atlas, "Help");
            help_title.dock = hi::TextDock::TopC;
            help_title.y = panel_y + 16.f;
            help_title.scale = 1.02f;
            DrawText(help_title);

            hi::PanelButtonDraw close_btn = ge::ui::SlotButton(false, false, is_dark_theme);
            close_btn.dock = hi::TextDock::TopL;
            close_btn.x = panel_left + panel_w - 102.f;
            close_btn.y = panel_y + 12.f;
            close_btn.w = 84.f;
            close_btn.h = 28.f;
            close_btn.style.border_radius = 8.f;
            const hi::PanelState close_state = PanelButton(close_btn);

            hi::TextDraw close_td = ge::ui::TextRegular(world_atlas, "Close");
            close_td.dock = hi::TextDock::TopL;
            close_td.x = close_btn.x + 16.f;
            close_td.y = close_btn.y + 6.f;
            close_td.scale = 0.64f;
            DrawText(close_td);

            io::StackOut<48> page_text{};
            page_text << "Page " << (help_window_page + 1u) << "/" << (total_pages == 0u ? 1u : total_pages);
            hi::TextDraw page_td = ge::ui::TextRegular(world_atlas, page_text.view());
            page_td.dock = hi::TextDock::TopC;
            page_td.y = panel_y + 42.f + help_content_offset_y;
            page_td.scale = 0.68f;
            page_td.style.a = 0.78f;
            DrawText(page_td);

            const io::u32 first_idx = help_window_page * per_page;
            for (io::u32 row = 0u; row < per_page; ++row) {
                const io::u32 cmd_idx = first_idx + row;
                if (cmd_idx >= total_cmds) break;
                const ChatCommandSpec& cmd = CHAT_COMMAND_SPECS[cmd_idx];

                const float y = panel_y + 78.f + help_content_offset_y + static_cast<float>(row) * 36.f;
                hi::TextDraw name_td = ge::ui::TextRegular(world_atlas, cmd.name);
                name_td.dock = hi::TextDock::TopL;
                name_td.x = panel_left + 20.f;
                name_td.y = y;
                name_td.scale = 0.78f;
                name_td.style.r = 0.84f;
                name_td.style.g = 0.92f;
                name_td.style.b = 1.f;
                DrawText(name_td);

                hi::TextDraw desc_td = ge::ui::TextRegular(world_atlas, cmd.description);
                desc_td.dock = hi::TextDock::TopL;
                desc_td.x = panel_left + 230.f;
                desc_td.y = y + 2.f;
                desc_td.scale = 0.68f;
                desc_td.style.a = 0.82f;
                DrawText(desc_td);
            }

            hi::PanelButtonDraw prev_btn = ge::ui::SlotButton(false, false, is_dark_theme);
            prev_btn.dock = hi::TextDock::TopL;
            prev_btn.x = panel_left + panel_w - 300.f;
            prev_btn.y = panel_y + panel_h - 52.f;
            prev_btn.w = 132.f;
            prev_btn.h = 34.f;
            prev_btn.style.border_radius = 10.f;
            hi::PanelState prev_state = PanelButton(prev_btn);
            if (prev_state.clicked && help_window_page > 0u)
                --help_window_page;

            hi::PanelButtonDraw next_btn = ge::ui::SlotButton(false, false, is_dark_theme);
            next_btn.dock = hi::TextDock::TopL;
            next_btn.x = panel_left + panel_w - 154.f;
            next_btn.y = panel_y + panel_h - 52.f;
            next_btn.w = 132.f;
            next_btn.h = 34.f;
            next_btn.style.border_radius = 10.f;
            hi::PanelState next_state = PanelButton(next_btn);
            if (next_state.clicked && help_window_page + 1u < total_pages)
                ++help_window_page;

            hi::TextDraw prev_td = ge::ui::TextRegular(world_atlas, "Previous");
            prev_td.dock = hi::TextDock::TopL;
            prev_td.x = prev_btn.x + 28.f;
            prev_td.y = prev_btn.y + 8.f;
            prev_td.scale = 0.62f;
            DrawText(prev_td);

            hi::TextDraw next_td = ge::ui::TextRegular(world_atlas, "Next");
            next_td.dock = hi::TextDock::TopL;
            next_td.x = next_btn.x + 46.f;
            next_td.y = next_btn.y + 8.f;
            next_td.scale = 0.62f;
            DrawText(next_td);

            if (close_state.clicked)
                CloseHelpWindow();
        }

        if (inventory_open) {
            const ge::item::Stack* tooltip_stack = InventoryTooltipStack();
            if (tooltip_stack) {
                const ge::item::Def& tooltip_def = ge::item::def(tooltip_stack->id);
                const float tooltip_w = 250.f;
                const float tooltip_h = 138.f;
                float tooltip_left = mouseX() + 18.f;
                float tooltip_top = mouseY() + 18.f;
                const float viewport_w = static_cast<float>(width());
                const float viewport_h = static_cast<float>(height());
                if (tooltip_left + tooltip_w > viewport_w - 12.f)
                    tooltip_left = viewport_w - tooltip_w - 12.f;
                if (tooltip_top + tooltip_h > viewport_h - 12.f)
                    tooltip_top = viewport_h - tooltip_h - 12.f;
                if (tooltip_left < 12.f) tooltip_left = 12.f;
                if (tooltip_top < 12.f) tooltip_top = 12.f;

                hi::PanelDraw tooltip_panel = ge::ui::PanelCard(is_dark_theme);
                tooltip_panel.dock = hi::TextDock::TopL;
                tooltip_panel.x = tooltip_left;
                tooltip_panel.y = tooltip_top;
                tooltip_panel.w = tooltip_w;
                tooltip_panel.h = tooltip_h;
                tooltip_panel.fill = is_dark_theme
                    ? ge::ui::Color(0.03f, 0.04f, 0.05f, 0.78f)
                    : ge::ui::Color(0.16f, 0.13f, 0.09f, 0.84f);
                tooltip_panel.border = is_dark_theme
                    ? ge::ui::Color(0.36f, 0.44f, 0.48f, 0.18f)
                    : ge::ui::Color(0.54f, 0.60f, 0.64f, 0.26f);
                DrawPanel(tooltip_panel);

                hi::TextDraw name_td = ge::ui::TextRegular(world_atlas, ge::item::name(tooltip_stack->id));
                name_td.dock = hi::TextDock::TopL;
                name_td.x = tooltip_left + 14.f;
                name_td.y = tooltip_top + 12.f;
                name_td.scale = 0.92f;
                name_td.style.outline = true;
                name_td.style.or_ = 0.01f;
                name_td.style.og_ = 0.01f;
                name_td.style.ob_ = 0.02f;
                name_td.style.oa_ = 0.92f;
                ge::ui::ApplyTextColor(name_td, ge::ui::ItemAccentColor(*tooltip_stack));
                DrawText(name_td);

                io::StackOut<128> line0{};
                line0 << "Type: " << InventoryTabLabel(tooltip_def.category);
                hi::TextDraw line0_td = ge::ui::TextRegular(world_atlas, line0.view());
                line0_td.dock = hi::TextDock::TopL;
                line0_td.x = tooltip_left + 14.f;
                line0_td.y = tooltip_top + 36.f;
                line0_td.scale = 0.70f;
                line0_td.style.a = 0.84f;
                DrawText(line0_td);

                io::StackOut<128> line1{};
                line1 << "Stack: " << tooltip_stack->count << "/" << tooltip_def.max_stack;
                hi::TextDraw line1_td = ge::ui::TextRegular(world_atlas, line1.view());
                line1_td.dock = hi::TextDock::TopL;
                line1_td.x = tooltip_left + 14.f;
                line1_td.y = tooltip_top + 56.f;
                line1_td.scale = 0.70f;
                line1_td.style.a = 0.84f;
                DrawText(line1_td);

                io::StackOut<128> line2{};
                if (ge::item::decays(tooltip_stack->id)) {
                    const io::u32 remaining_ms = EstimateStackDecayRemainingMs(*tooltip_stack);
                    const io::u32 remaining_sec = (remaining_ms + 999u) / 1000u;
                    line2 << "Decay: " << remaining_sec << "s";
                    hi::TextDraw line2_td = ge::ui::TextRegular(world_atlas, line2.view());
                    line2_td.dock = hi::TextDock::TopL;
                    line2_td.x = tooltip_left + 14.f;
                    line2_td.y = tooltip_top + 76.f;
                    line2_td.scale = 0.70f;
                    ge::ui::ApplyTextColor(line2_td, ge::ui::ItemAccentColor(*tooltip_stack));
                    DrawText(line2_td);

                    io::StackOut<128> line3{};
                    switch (ge::item::freshness_band(*tooltip_stack)) {
                    case ge::item::FreshnessBand::Fresh: line3 << "State: Fresh"; break;
                    case ge::item::FreshnessBand::Stale: line3 << "State: Half-spoiled"; break;
                    default: line3 << "State: Critical"; break;
                    }
                    hi::TextDraw line3_td = ge::ui::TextRegular(world_atlas, line3.view());
                    line3_td.dock = hi::TextDock::TopL;
                    line3_td.x = tooltip_left + 14.f;
                    line3_td.y = tooltip_top + 96.f;
                    line3_td.scale = 0.70f;
                    line3_td.style.a = 0.84f;
                    DrawText(line3_td);
                } else if (ge::item::is_placeable(tooltip_stack->id)) {
                    line2 << "Property: Placeable block";
                    hi::TextDraw line2_td = ge::ui::TextRegular(world_atlas, line2.view());
                    line2_td.dock = hi::TextDock::TopL;
                    line2_td.x = tooltip_left + 14.f;
                    line2_td.y = tooltip_top + 76.f;
                    line2_td.scale = 0.70f;
                    line2_td.style.a = 0.84f;
                    DrawText(line2_td);
                } else if (ge::item::is_consumable(tooltip_stack->id)) {
                    line2 << "Hunger: +" << tooltip_def.hunger_gain;
                    hi::TextDraw line2_td = ge::ui::TextRegular(world_atlas, line2.view());
                    line2_td.dock = hi::TextDock::TopL;
                    line2_td.x = tooltip_left + 14.f;
                    line2_td.y = tooltip_top + 76.f;
                    line2_td.scale = 0.70f;
                    line2_td.style.a = 0.84f;
                    DrawText(line2_td);

                    if (tooltip_def.poison_ms > 0u) {
                        io::StackOut<128> line3{};
                        line3 << "Spoiled effect: poison";
                        hi::TextDraw line3_td = ge::ui::TextRegular(world_atlas, line3.view());
                        line3_td.dock = hi::TextDock::TopL;
                        line3_td.x = tooltip_left + 14.f;
                        line3_td.y = tooltip_top + 96.f;
                        line3_td.scale = 0.70f;
                        line3_td.style.a = 0.84f;
                        DrawText(line3_td);
                    }
                } else if (tooltip_def.category == ge::item::Category::SpellingWards) {
                    line2 << "Property: Ward focus item";
                    hi::TextDraw line2_td = ge::ui::TextRegular(world_atlas, line2.view());
                    line2_td.dock = hi::TextDock::TopL;
                    line2_td.x = tooltip_left + 14.f;
                    line2_td.y = tooltip_top + 76.f;
                    line2_td.scale = 0.70f;
                    line2_td.style.a = 0.84f;
                    DrawText(line2_td);
                } else if (tooltip_def.category == ge::item::Category::Spells) {
                    line2 << "Property: Spell casting item";
                    hi::TextDraw line2_td = ge::ui::TextRegular(world_atlas, line2.view());
                    line2_td.dock = hi::TextDock::TopL;
                    line2_td.x = tooltip_left + 14.f;
                    line2_td.y = tooltip_top + 76.f;
                    line2_td.scale = 0.70f;
                    line2_td.style.a = 0.84f;
                    DrawText(line2_td);
                }
            }
        }

        if (inventory_open && InventoryCursorActive()) {
            const float cursor_x = mouseX();
            const float cursor_y = mouseY();
            const float cursor_size = 42.f;
            float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
            if (gui_texture_atlas >= 0 && ItemIconUv(inventory_state.cursor, u0, v0, u1, v1)) {
                hi::ImageDraw image{};
                image.atlas = gui_texture_atlas;
                image.dock = hi::TextDock::TopL;
                image.x = cursor_x + 12.f;
                image.y = cursor_y + 8.f;
                image.w = cursor_size;
                image.h = cursor_size;
                image.u0 = u0;
                image.v0 = v0;
                image.u1 = u1;
                image.v1 = v1;
                image.tint = ge::ui::Color(1.f, 1.f, 1.f, 1.f);
                DrawImage(image);
            }
            io::StackOut<16> count_text{};
            count_text << inventory_state.cursor.count;
            hi::TextDraw count_td = ge::ui::TextRegular(world_atlas, count_text.view());
            count_td.dock = hi::TextDock::TopL;
            count_td.x = cursor_x + 42.f;
            count_td.y = cursor_y + 30.f;
            count_td.scale = 0.70f;
            count_td.style.outline = true;
            count_td.style.or_ = 0.02f;
            count_td.style.og_ = 0.02f;
            count_td.style.ob_ = 0.04f;
            count_td.style.oa_ = 0.95f;
            DrawText(count_td);
        }

        io::u32 chat_snapshot_head = 0u;
        io::u32 chat_snapshot_count = 0u;
        chat_log_lock.lock();
        if (chat_log) {
            chat_snapshot_head = chat_log_head;
            chat_snapshot_count = chat_log_count;
            for (io::u32 i = 0; i < CHAT_LOG_CAP; ++i)
                chat_render_snapshot[i] = chat_log[i];
        }
        chat_log_lock.unlock();

        const bool hide_chat_lines_for_suggestions =
            chat_open && (chat_suggestion_count > 0u || chat_arg_help_count > 0u);
        if (!hide_chat_lines_for_suggestions && (chat_snapshot_count > 0u || chat_open)) {
            const io::u32 chat_visible = chat_open ? 10u : 6u;
            const io::u32 now_chat_ms32 = static_cast<io::u32>(io::monotonic_ms());
            io::u32 draw_idx[CHAT_LOG_CAP]{};
            float draw_alpha[CHAT_LOG_CAP]{};
            io::u32 draw_count = 0u;
            for (io::u32 i = 0u; i < chat_snapshot_count && draw_count < chat_visible; ++i) {
                const io::u32 idx = (chat_snapshot_head + CHAT_LOG_CAP - 1u - i) % CHAT_LOG_CAP;
                const HudChatLine& ln = chat_render_snapshot[idx];
                const float alpha = ChatLineAlpha(ln, chat_open, now_chat_ms32);
                if (alpha <= 0.f)
                    continue;
                draw_idx[draw_count] = idx;
                draw_alpha[draw_count] = alpha;
                ++draw_count;
            }
            for (io::u32 i = 0u; i < draw_count; ++i) {
                const io::u32 idx = draw_idx[i];
                const float line_alpha = draw_alpha[i];
                const HudChatLine& ln = chat_render_snapshot[idx];
                io::StackOut<300> line_text{};
                if (ln.kind == ge::net::CHAT_KIND_SERVER) {
                    line_text << "[SERVER] " << io::char_view{ ln.text, ln.text_len };
                } else if (ln.name_len > 0u) {
                    line_text << "[" << io::char_view{ ln.name, ln.name_len } << "] " << io::char_view{ ln.text, ln.text_len };
                } else {
                    line_text << io::char_view{ ln.text, ln.text_len };
                }

                hi::TextDraw line_td = ge::ui::TextRegular(world_atlas, line_text.view());
                line_td.dock = hi::TextDock::BottomL;
                line_td.x = 12.f;
                line_td.y = -56.f - static_cast<float>((draw_count - 1u - i) * 22u) - (chat_open ? 30.f : 0.f);
                line_td.scale = 0.74f;
                if (ln.kind == ge::net::CHAT_KIND_SERVER) {
                    line_td.style.r = 1.f;
                    line_td.style.g = 0.72f;
                    line_td.style.b = 0.28f;
                }
                line_td.style.a *= line_alpha;
                DrawText(line_td);
            }
        }

        if (chat_open) {
            hi::TextDraw prompt_td = ge::ui::TextRegular(world_atlas, ">");
            prompt_td.dock = hi::TextDock::BottomL;
            prompt_td.x = 12.f;
            prompt_td.y = -56.f;
            prompt_td.scale = 0.78f;
            DrawText(prompt_td);

            hi::TextFieldDraw chat_field = ge::ui::TextInputRegular(
                world_atlas,
                io::char_view_mut{ chat_input_utf8, sizeof(chat_input_utf8) },
                &chat_input_len,
                CHAT_TEXT_FIELD_ID);
            chat_field.dock = hi::TextDock::BottomL;
            chat_field.x = 48.f;
            chat_field.y = -56.f;
            chat_field.scale = 0.78f;
            chat_field.style.placeholder = "Type a message...";
            chat_field.style.min_width = static_cast<float>(width()) * 0.75f;

            const hi::TextFieldState state = TextField(chat_field);
            if (!state.active)
                FocusTextField(CHAT_TEXT_FIELD_ID);
            UpdateChatSuggestions();
            if (state.submitted)
                SubmitChatInput();

            if (chat_suggestion_count > 0u) {
                for (io::u32 i = 0u; i < chat_suggestion_count; ++i) {
                    io::StackOut<192> suggestion{};
                    suggestion << io::char_view{ chat_suggestion_text[i], chat_suggestion_text_len[i] }
                               << " - "
                               << io::char_view{ chat_suggestion_desc[i], chat_suggestion_desc_len[i] };
                    const bool has_icon = (chat_suggestion_icon_item[i] != ge::item::Id::None);
                    const float row_y = -96.f - static_cast<float>(i) * 20.f;
                    if (has_icon && gui_texture_atlas >= 0) {
                        ge::item::Stack icon_stack{};
                        icon_stack.id = chat_suggestion_icon_item[i];
                        icon_stack.count = 1u;
                        icon_stack.freshness = ge::item::FRESHNESS_MAX;
                        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
                        if (ItemIconUv(icon_stack, u0, v0, u1, v1)) {
                            hi::ImageDraw icon{};
                            icon.atlas = gui_texture_atlas;
                            icon.dock = hi::TextDock::BottomL;
                            icon.x = 48.f;
                            icon.y = row_y;
                            icon.w = 16.f;
                            icon.h = 16.f;
                            icon.u0 = u0;
                            icon.v0 = v0;
                            icon.u1 = u1;
                            icon.v1 = v1;
                            icon.tint = ge::ui::Color(1.f, 1.f, 1.f, 1.f);
                            DrawImage(icon);
                        }
                    }
                    hi::TextDraw td = ge::ui::TextRegular(world_atlas, suggestion.view());
                    td.dock = hi::TextDock::BottomL;
                    td.x = has_icon ? 70.f : 48.f;
                    td.y = row_y;
                    td.scale = (i == chat_suggestion_selected) ? 0.82f : 0.72f;
                    if (i == chat_suggestion_selected) {
                        td.style.r = 1.f;
                        td.style.g = 0.85f;
                        td.style.b = 0.42f;
                    }
                    DrawText(td);
                }
            }

            if (chat_arg_help_count > 0u) {
                for (io::u32 i = 0u; i < chat_arg_help_count; ++i) {
                    hi::TextDraw td = ge::ui::TextRegular(world_atlas, io::char_view{ chat_arg_help[i], chat_arg_help_len[i] });
                    td.dock = hi::TextDock::BottomR;
                    td.x = -18.f;
                    td.y = -126.f - static_cast<float>(i) * 18.f;
                    td.scale = 0.68f;
                    td.style.r = 0.72f;
                    td.style.g = 0.88f;
                    td.style.b = 1.f;
                    DrawText(td);
                }
            }
        }
    }
