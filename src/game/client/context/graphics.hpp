#pragma once

    inline void RenderGraphicsGui() noexcept {
        DrawScreenTitle("GRAPHICS");

        io::StackOut<320> info{};
        const io::u32 max_rd = MaxSafeRenderDistance();
        info << "CPU logical threads: " << hw_threads
             << "\nMesh workers configured: " << mesh_workers_configured
             << " of " << mesh_worker_max
             << "\nMesh workers running: " << mesh_worker_threads
             << "\nNetwork: " << NetStateText() << "\n"
             << "Applied render radius: " << render_distance_chunks
             << "\nRender radius max safe: " << max_rd;
        DrawTopLeftText(info.view(), 82.f, 0.82f);

        if (hw_threads < 4u) {
            hi::TextDraw warn = ge::ui::TextRegular(world_atlas,
                "Warning: game minimum is 4 logical CPU threads, otherwise lags/freezes are expected.");
            warn.dock = hi::TextDock::TopC;
            warn.y = 220.f;
            warn.scale = 0.84f;
            warn.style.r = 1.f;
            warn.style.g = 0.55f;
            warn.style.b = 0.28f;
            warn.style.a = 1.f;
            DrawText(warn);
        }

        io::StackOut<128> rd_text{};
        rd_text << "Render Distance (chunks radius): " << io::to_u32(render_distance_pending + 0.5f);
        const float rd_max = static_cast<float>(MaxSafeRenderDistance());
        hi::SliderDraw rd = ge::ui::SliderHeader(world_atlas, rd_text.view(),
            &render_distance_pending, 1.f, rd_max, 1.f, 92001u);
        rd.dock = hi::TextDock::TopC;
        rd.y = 286.f;
        (void)Slider(rd);

        io::StackOut<128> wt_text{};
        wt_text << "Mesh workers: " << io::to_u32(mesh_workers_pending + 0.5f)
                << " / max " << mesh_worker_max;
        hi::SliderDraw wt = ge::ui::SliderHeader(world_atlas, wt_text.view(),
            &mesh_workers_pending, static_cast<float>(mesh_worker_min),
            static_cast<float>(mesh_worker_max), 1.f, 92002u);
        wt.dock = hi::TextDock::TopC;
        wt.y = 366.f;
        (void)Slider(wt);

        if (MenuButton("Apply", static_cast<float>(height()) - 172.f, false).clicked)
            ApplyGraphicsSettings();
        if (MenuButton("Back", static_cast<float>(height()) - 120.f, false).clicked)
            screen = ScreenState::Settings;
    }
