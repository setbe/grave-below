#pragma once

    inline void RenderSettingsGui() noexcept {
        DrawScreenTitle("SETTINGS");

        io::StackOut<360> info{};
        info.reset();
        info << "Engine version: " << ge::ENGINE_VERSION << '\n'
             << "Game version: " << ge::GAME_VERSION << '\n'
             << "Key bindings: " << ge::key_bindings_state_to_string(key_bindings_meta.state) << "\n\n"
             << "CPU logical threads: " << hw_threads << "\n"
             << "Mesh workers configured: " << mesh_workers_configured << '\n'
             << "Network: " << NetStateText() << "\n\n"
             << "Settings groups:\n"
             << "- Audio\n"
             << "- Graphics\n"
             << "- Keyboard\n\n";
        WriteSessionContext(info);

        DrawTopLeftText(info.view(), 78.f, 0.85f);

        const float row_y = 250.f;
        if (MenuButton("Audio", row_y).clicked) {}
        if (MenuButton("Graphics", row_y + 48.f).clicked) screen = ScreenState::Graphics;
        io::StackOut<64> theme_text{};
        theme_text << "Dark Theme: " << (is_dark_theme ? "On" : "Off");
        if (MenuButton(theme_text.view(), row_y + 96.f).clicked) {
            is_dark_theme = !is_dark_theme;
            SaveRuntimeConfig();
        }
        if (MenuButton("Keyboard", row_y + 144.f).clicked) {
            screen = ScreenState::KeyBindings;
            CancelRebind();
        }

        if (MenuButton("Back", static_cast<float>(height()) - 120.f, false).clicked) {
            CancelRebind();
            screen = ScreenState::MainMenu;
        }
    }
