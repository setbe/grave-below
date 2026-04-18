#pragma once

    inline void RenderKeyBindingsGui() noexcept {
        DrawScreenTitle("KEYBOARD");

        const float step = 34.f;
        const float start_y = -0.5f * (static_cast<float>(ACTION_COUNT - 1u) * step);
        float y = start_y;
        for (io::usize i = 0; i < ACTION_COUNT; ++i) {
            const Action action = ActionByIndex(i);
            io::StackOut<220> label{};
            label.reset();
            label << ActionName(action) << ": ";
            AppendBindingText(label, key_bindings[i]);
            if (setting_rebind && rebind_action_index == i) {
                if (rebind_step == 1) label << "   [press first key]";
                else label << "   [second key / Enter to finish]";
            }

            hi::ButtonDraw btn = ge::ui::ButtonRegular(world_atlas, label.view());
            btn.dock = hi::TextDock::LeftC;
            btn.x = 170.f;
            btn.y = y;
            if (Button(btn).clicked)
                StartRebind(action);

            hi::ButtonDraw reset_btn = ge::ui::ButtonRegular(world_atlas, "Reset");
            reset_btn.dock = hi::TextDock::LeftC;
            reset_btn.x = 20.f;
            reset_btn.y = y;
            reset_btn.scale = 0.74f;
            if (Button(reset_btn).clicked)
                ResetBinding(action);
            y += step;
        }

        io::StackOut<240> hint{};
        hint.reset();
        if (setting_rebind)
            hint << "Rebinding: Esc cancel, press same key again for 1-key, or press second key for combo";
        else
            hint << "Click action to rebind (one key or key1 + key2). Reset is placed left for each action.";

        hi::TextDraw td = ge::ui::TextRegular(world_atlas, hint.view());
        td.dock = hi::TextDock::BottomL;
        td.x = 20.f;
        td.y = -ge::FONT_PIXEL_HEIGHT;
        td.scale = 0.72f;
        DrawText(td);

        if (MenuButton("Reset All", static_cast<float>(height()) - 172.f, false).clicked)
            ResetAllBindings();

        if (MenuButton("Back", static_cast<float>(height()) - 120.f, false).clicked) {
            CancelRebind();
            screen = ScreenState::Settings;
        }
    }
