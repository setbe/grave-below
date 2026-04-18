#pragma once

    inline void RenderMultiplayerGui() noexcept {
        DrawScreenTitle("MULTIPLAYER");

        io::StackOut<320> meta{};
        meta << "Server list: " << ge::server_list_state_to_string(server_list_meta.state)
             << "\nEntries: " << server_list.size()
             << "\nSelected: " << (server_selected_index + 1u);
        DrawTopLeftText(meta.view(), 76.f, 0.78f);

        float list_y = 136.f;
        for (io::usize i = 0; i < server_list.size(); ++i) {
            const ge::ServerListEntry& e = server_list[i];
            io::StackOut<256> row_text{};
            row_text << (i + 1u) << ". " << io::char_view{ e.name_utf8 }
                     << "  (" << io::char_view{ e.ip_utf8 } << ":" << static_cast<io::u32>(e.port) << ")";

            hi::ButtonDraw row = (i == server_selected_index)
                ? ge::ui::ButtonHeader(world_atlas, row_text.view())
                : ge::ui::ButtonRegular(world_atlas, row_text.view());
            row.dock = hi::TextDock::TopC;
            row.y = list_y;
            if (Button(row).clicked) {
                server_selected_index = i;
                SyncServerFormFromSelected();
            }
            list_y += 36.f;
            if (list_y > static_cast<float>(height()) - 280.f)
                break;
        }

        ClampServerFormLens();

        hi::TextDraw form_title = ge::ui::TextRegular(world_atlas, "Selected server:");
        form_title.dock = hi::TextDock::TopL;
        form_title.x = 20.f;
        form_title.y = static_cast<float>(height()) - 250.f;
        form_title.scale = 0.75f;
        DrawText(form_title);

        hi::TextFieldDraw name_field = ge::ui::TextInputRegular(
            world_atlas, io::char_view_mut{ server_name_input, sizeof(server_name_input) }, &server_name_input_len, 93001u);
        name_field.dock = hi::TextDock::TopL;
        name_field.x = 20.f;
        name_field.y = static_cast<float>(height()) - 220.f;
        name_field.style.placeholder = "Server name (max 32 bytes)";
        (void)TextField(name_field);

        hi::TextFieldDraw ip_field = ge::ui::TextInputRegular(
            world_atlas, io::char_view_mut{ server_ip_input, sizeof(server_ip_input) }, &server_ip_input_len, 93002u);
        ip_field.dock = hi::TextDock::TopL;
        ip_field.x = 20.f;
        ip_field.y = static_cast<float>(height()) - 182.f;
        ip_field.style.placeholder = "IPv4 address";
        (void)TextField(ip_field);

        hi::TextFieldDraw port_field = ge::ui::TextInputRegular(
            world_atlas, io::char_view_mut{ server_port_input, sizeof(server_port_input) }, &server_port_input_len, 93003u);
        port_field.dock = hi::TextDock::TopL;
        port_field.x = 20.f;
        port_field.y = static_cast<float>(height()) - 144.f;
        port_field.style.placeholder = "25565";
        (void)TextField(port_field);

        hi::ButtonDraw connect_btn = ge::ui::ButtonHeader(world_atlas, "Connect");
        connect_btn.dock = hi::TextDock::BottomC;
        connect_btn.y = -186.f;
        if (Button(connect_btn).clicked)
            (void)BeginConnectSelectedServer();

        hi::ButtonDraw add_btn = ge::ui::ButtonRegular(world_atlas, "Add");
        add_btn.dock = hi::TextDock::BottomC;
        add_btn.x = -236.f;
        add_btn.y = -142.f;
        if (Button(add_btn).clicked)
            AddServerFromForm();

        hi::ButtonDraw update_btn = ge::ui::ButtonRegular(world_atlas, "Update");
        update_btn.dock = hi::TextDock::BottomC;
        update_btn.x = -120.f;
        update_btn.y = -142.f;
        if (Button(update_btn).clicked)
            UpdateSelectedServerFromForm();

        hi::ButtonDraw remove_btn = ge::ui::ButtonRegular(world_atlas, "Remove");
        remove_btn.dock = hi::TextDock::BottomC;
        remove_btn.x = -4.f;
        remove_btn.y = -142.f;
        if (Button(remove_btn).clicked)
            RemoveSelectedServer();

        hi::ButtonDraw up_btn = ge::ui::ButtonRegular(world_atlas, "Move Up");
        up_btn.dock = hi::TextDock::BottomC;
        up_btn.x = 128.f;
        up_btn.y = -142.f;
        if (Button(up_btn).clicked)
            MoveSelectedServerUp();

        hi::ButtonDraw down_btn = ge::ui::ButtonRegular(world_atlas, "Move Down");
        down_btn.dock = hi::TextDock::BottomC;
        down_btn.x = 260.f;
        down_btn.y = -142.f;
        if (Button(down_btn).clicked)
            MoveSelectedServerDown();

        hi::ButtonDraw back_btn = ge::ui::ButtonRegular(world_atlas, "Back");
        back_btn.dock = hi::TextDock::BottomC;
        back_btn.y = -96.f;
        if (Button(back_btn).clicked) {
            CancelRebind();
            screen = ScreenState::MainMenu;
        }
    }
