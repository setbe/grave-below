#pragma once

    inline void RenderMainMenuGui(float dt) noexcept {
        const float sw = static_cast<float>(width());
        const float sh = static_cast<float>(height());
        const float t = frame.scene_time;
        const float pulse = 0.5f + 0.5f * lm::sinf(t * 0.55f);

        hi::PanelDraw bg{};
        bg.dock = hi::TextDock::TopL;
        bg.x = 0.f;
        bg.y = 0.f;
        bg.w = sw;
        bg.h = sh;
        bg.style.border = false;
        bg.fill = is_dark_theme
            ? ge::ui::Color(0.03f, 0.05f, 0.07f, 0.70f)
            : ge::ui::Color(0.16f, 0.14f, 0.12f, 0.58f);
        bg.border = ge::ui::Color(0.f, 0.f, 0.f, 0.f);
        DrawPanel(bg);

        for (io::u32 i = 0u; i < 6u; ++i) {
            const float fi = static_cast<float>(i);
            const float drift_x = lm::sinf(t * (0.18f + fi * 0.02f) + fi * 1.37f) * (66.f + fi * 18.f);
            const float drift_y = lm::cosf(t * (0.14f + fi * 0.03f) + fi * 0.91f) * (14.f + fi * 4.f);

            hi::PanelDraw band{};
            band.dock = hi::TextDock::TopC;
            band.x = drift_x;
            band.y = 84.f + fi * 70.f + drift_y;
            band.w = sw * (0.98f + fi * 0.05f);
            band.h = 54.f + fi * 8.f;
            band.style.border = false;
            band.style.border_radius = 26.f + fi * 3.f;
            if (is_dark_theme) {
                band.fill = ge::ui::Color(0.10f + fi * 0.02f, 0.27f + fi * 0.03f, 0.30f + fi * 0.02f,
                                          0.08f + 0.02f * pulse);
            } else {
                band.fill = ge::ui::Color(0.30f + fi * 0.02f, 0.24f + fi * 0.02f, 0.12f + fi * 0.02f,
                                          0.06f + 0.02f * pulse);
            }
            band.border = ge::ui::Color(0.f, 0.f, 0.f, 0.f);
            DrawPanel(band);
        }

        for (io::u32 i = 0u; i < 34u; ++i) {
            const float fi = static_cast<float>(i);
            const float speed = 0.012f + static_cast<float>(i % 7u) * 0.003f;
            float u = 0.13f * fi + t * speed;
            while (u >= 1.f) u -= 1.f;
            while (u < 0.f) u += 1.f;
            const float x = 24.f + u * (sw - 48.f);
            float y_phase = 37.f * fi + t * (12.f + static_cast<float>(i % 5u) * 2.f);
            const float y_span = sh - 170.f;
            if (y_span > 1.f) {
                while (y_phase >= y_span) y_phase -= y_span;
                while (y_phase < 0.f) y_phase += y_span;
            } else {
                y_phase = 0.f;
            }
            const float y = 86.f + y_phase;
            const float s = 2.f + static_cast<float>(i % 4u);

            hi::PanelDraw spark{};
            spark.dock = hi::TextDock::TopL;
            spark.x = x;
            spark.y = y;
            spark.w = s;
            spark.h = s;
            spark.style.border = false;
            spark.style.border_radius = s;
            spark.fill = is_dark_theme
                ? ge::ui::Color(0.76f, 0.98f, 0.92f, 0.18f + 0.06f * pulse)
                : ge::ui::Color(0.95f, 0.98f, 0.88f, 0.16f + 0.05f * pulse);
            spark.border = ge::ui::Color(0.f, 0.f, 0.f, 0.f);
            DrawPanel(spark);
        }

        hi::PanelDraw ring{};
        ring.dock = hi::TextDock::TopC;
        ring.x = lm::sinf(t * 0.22f) * 44.f;
        ring.y = 124.f + lm::cosf(t * 0.17f) * 18.f;
        ring.w = sw * 0.62f;
        ring.h = 168.f;
        ring.style.border = true;
        ring.style.border_radius = 84.f;
        ring.fill = ge::ui::Color(0.f, 0.f, 0.f, 0.f);
        ring.border = is_dark_theme
            ? ge::ui::Color(0.50f, 0.88f, 0.82f, 0.16f)
            : ge::ui::Color(0.96f, 0.82f, 0.42f, 0.14f);
        ring.border_px = 2.f;
        DrawPanel(ring);

        DrawScreenTitle("GRAVE BELOW");

        io::StackOut<256> ctx{};
        ctx.reset();
        WriteSessionContext(ctx);
        hi::TextDraw td = ge::ui::TextRegular(world_atlas, ctx.view());
        td.dock = hi::TextDock::TopC;
        td.y = 102.f;
        td.scale = 0.8f;
        DrawText(td);

        hi::TextFieldDraw player_name = ge::ui::TextInputHeader(
            world_atlas,
            io::char_view_mut{ player_name_utf8, sizeof(player_name_utf8) },
            &player_name_len,
            91001u);
        player_name.dock = hi::TextDock::TopC;
        player_name.y = 156.f;
        player_name.style.placeholder = "Player";
        (void)TextField(player_name);
        UpdatePlayerNameIfChanged();

        const float start_y = 214.f;
        const float step = ge::FONT_PIXEL_HEIGHT + 24.f;

        if (MenuButton("Singleplayer", start_y + step * 1.f).clicked) EnterSingleplayer();
        if (MenuButton("Multiplayer", start_y + step * 2.f).clicked) EnterMultiplayer();
        if (MenuButton("Settings",    start_y + step * 3.f).clicked) screen = ScreenState::Settings;
        if (MenuButton("Quit the game", start_y + step * 4.f).clicked) frame.request_quit = true;

        frame.dt_history.push(dt);
        io::StackOut<192> stats{};
        stats.reset();
        stats << "FPS(avg): " << io::to_u32(frame.dt_history.avg_fps()) << "\nVSync: " << isVSync();
        hi::TextDraw tr = ge::ui::TextRegular(world_atlas, stats.view());
        tr.dock = hi::TextDock::TopR;
        DrawText(tr);
    }
