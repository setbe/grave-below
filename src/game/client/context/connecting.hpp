#pragma once

    inline void RenderConnectingGui() noexcept {
        if (net_state.load() == 2u && screen == ScreenState::Connecting)
            EnterInGameConnected();

        DrawScreenTitle("CONNECTING");

        io::StackOut<384> status{};
        status << "Player: " << PlayerNameView() << "\n";
        if (session.server_name_len > 0) status << "Server: " << SessionServerNameView() << "\n";
        if (session.endpoint_len > 0) status << "Endpoint: " << SessionEndpointView() << "\n";
        status << "\nNetwork state: " << NetStateText()
               << "\nHandshake attempts: " << net_handshake_attempts.load()
               << "\nLast drop reason: " << net_last_drop_reason.load()
               << "\nLast disconnect reason: " << net_last_disconnect_reason.load();
        if (net_state.load() != 2u) {
            status << "\n\nCannot connect yet.\n"
                   << "Make sure file \"server.exe\" is running,\n"
                   << "then keep this screen open or press Back.";
        }
        DrawTopLeftText(status.view(), 88.f, 0.82f);

        hi::ButtonDraw back_btn = ge::ui::ButtonRegular(world_atlas, "Back");
        back_btn.dock = hi::TextDock::BottomC;
        back_btn.y = -96.f;
        if (Button(back_btn).clicked) {
            StopConnect();
            if (session.mode == SessionMode::Singleplayer) {
                session.mode = SessionMode::None;
                ClearSessionText();
                screen = ScreenState::MainMenu;
            } else {
                screen = ScreenState::Multiplayer;
            }
        }
    }
