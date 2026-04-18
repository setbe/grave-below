#pragma once

    inline void RenderInGameDead(float dt) noexcept {
        frame.dt_history.push(dt);

        if (MenuButton("Respawn", 180.f).clicked) {
            if (!SendModeCommandToServer("/respawn"))
                PushSystemChat("Failed to request respawn");
        }

        if (MenuButton("Leave the server", 250.f, false).clicked) {
            ReturnToMainMenu();
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
            if (state.submitted)
                SubmitChatInput();
        }
    }
