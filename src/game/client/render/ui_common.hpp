#pragma once

    inline float PushDtSample(float dt) noexcept {
        frame.dt_history.push(dt);
        return frame.dt_history.avg_fps();
    }

    inline hi::ButtonState MenuButton(io::char_view text, float y, bool header = true) noexcept {
        hi::ButtonDraw button = header
            ? ge::ui::ButtonHeader(world_atlas, text)
            : ge::ui::ButtonRegular(world_atlas, text);
        button.dock = hi::TextDock::TopC;
        button.x = 0.f;
        button.y = y;
        return Button(button);
    }

    inline void DrawScreenTitle(io::char_view title) noexcept {
        hi::TextDraw td = ge::ui::TextHeader(world_atlas, title);
        td.dock = hi::TextDock::TopC;
        td.y = 24.f;
        DrawText(td);
    }

    inline void DrawTopLeftText(io::char_view text, float y, float scale) noexcept {
        hi::TextDraw td = ge::ui::TextRegular(world_atlas, text);
        td.dock = hi::TextDock::TopL;
        td.x = 20.f;
        td.y = y;
        td.scale = scale;
        DrawText(td);
    }

// Screen context implementations are split by module:
