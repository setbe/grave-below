#pragma once

    struct BlockRayHit {
        bool hit = false;
        io::i32 wx = 0;
        io::i32 wy = 0;
        io::i32 wz = 0;
        io::i32 prev_wx = 0;
        io::i32 prev_wy = 0;
        io::i32 prev_wz = 0;
    };

    IO_NODISCARD inline bool RaycastSolidBlock(BlockRayHit& out,
                                               float max_distance = 8.f,
                                               float step = 0.10f) const noexcept {
        out = {};
        if (!chunk_world_ready) return false;
        if (max_distance <= 0.f || step <= 0.f) return false;

        const float ox = camera.position[0];
        const float oy = camera.position[1];
        const float oz = camera.position[2];
        const float dx = camera.front[0];
        const float dy = camera.front[1];
        const float dz = camera.front[2];

        io::i32 prev_x = floor_to_i32(ox);
        io::i32 prev_y = floor_to_i32(oy);
        io::i32 prev_z = floor_to_i32(oz);

        for (float t = step; t <= max_distance; t += step) {
            const io::i32 wx = floor_to_i32(ox + dx * t);
            const io::i32 wy = floor_to_i32(oy + dy * t);
            const io::i32 wz = floor_to_i32(oz + dz * t);
            if (wx == prev_x && wy == prev_y && wz == prev_z)
                continue;

            const ge::voxel::BlockId id = voxel_world.get_world_block(wx, wy, wz);
            if (id != ge::voxel::BlockId::Air && !ge::voxel::is_liquid(id)) {
                out.hit = true;
                out.wx = wx;
                out.wy = wy;
                out.wz = wz;
                out.prev_wx = prev_x;
                out.prev_wy = prev_y;
                out.prev_wz = prev_z;
                return true;
            }

            prev_x = wx;
            prev_y = wy;
            prev_z = wz;
        }
        return false;
    }

    IO_NODISCARD inline bool SubmitBlockEdit(io::i32 wx, io::i32 wy, io::i32 wz,
                                             ge::voxel::BlockId block_id,
                                             io::u16 block_state) noexcept {
        if (screen != ScreenState::InGame) return false;
        if (net_state.load() != 2u) return false;
        if (!EnqueueNetBlockEdit(wx, wy, wz, block_id, block_state))
            return false;
        return true;
    }

    IO_NODISCARD inline bool FindSolidBlockInsidePlayer(io::i32& out_wx, io::i32& out_wy, io::i32& out_wz) const noexcept {
        if (!chunk_world_ready) return false;
        static constexpr float PLAYER_RADIUS = 0.32f;
        static constexpr float EPS = 0.001f;
        const float player_height = player_crawling ? PlayerCrawlHeight() : PlayerStandHeight();
        const float player_eye_to_feet = player_crawling ? PlayerCrawlEyeToFeet() : PlayerStandEyeToFeet();

        const float min_x = camera.position[0] - PLAYER_RADIUS;
        const float max_x = camera.position[0] + PLAYER_RADIUS;
        const float min_y = camera.position[1] - player_eye_to_feet;
        const float max_y = min_y + player_height;
        const float min_z = camera.position[2] - PLAYER_RADIUS;
        const float max_z = camera.position[2] + PLAYER_RADIUS;

        const io::i32 x0 = floor_to_i32(min_x + EPS);
        const io::i32 x1 = floor_to_i32(max_x - EPS);
        const io::i32 y0 = floor_to_i32(min_y + EPS);
        const io::i32 y1 = floor_to_i32(max_y - EPS);
        const io::i32 z0 = floor_to_i32(min_z + EPS);
        const io::i32 z1 = floor_to_i32(max_z - EPS);

        for (io::i32 y = y0; y <= y1; ++y)
            for (io::i32 z = z0; z <= z1; ++z)
                for (io::i32 x = x0; x <= x1; ++x)
                    if (IsSolidWorldCell(x, y, z)) {
                        out_wx = x;
                        out_wy = y;
                        out_wz = z;
                        return true;
                    }
        return false;
    }

    IO_NODISCARD inline bool IsBlockOnPlayerHeadLevel(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
        // Allow placing inside body, but disallow placing into head/eye volume.
        static constexpr float HEAD_RADIUS = 0.22f;
        static constexpr float HEAD_HALF_HEIGHT = 0.22f;
        static constexpr float EPS = 0.001f;

        const float min_x = camera.position[0] - HEAD_RADIUS;
        const float max_x = camera.position[0] + HEAD_RADIUS;
        const float min_y = camera.position[1] - HEAD_HALF_HEIGHT;
        const float max_y = camera.position[1] + HEAD_HALF_HEIGHT;
        const float min_z = camera.position[2] - HEAD_RADIUS;
        const float max_z = camera.position[2] + HEAD_RADIUS;

        const io::i32 x0 = floor_to_i32(min_x + EPS);
        const io::i32 x1 = floor_to_i32(max_x - EPS);
        const io::i32 y0 = floor_to_i32(min_y + EPS);
        const io::i32 y1 = floor_to_i32(max_y - EPS);
        const io::i32 z0 = floor_to_i32(min_z + EPS);
        const io::i32 z1 = floor_to_i32(max_z - EPS);

        return wx >= x0 && wx <= x1 &&
               wy >= y0 && wy <= y1 &&
               wz >= z0 && wz <= z1;
    }

    inline void BreakTargetedBlock() noexcept {
        if (IsPlayerBodyCollidingAt(camera.position)) {
            io::i32 wx = 0, wy = 0, wz = 0;
            if (FindSolidBlockInsidePlayer(wx, wy, wz)) {
                (void)SubmitBlockEdit(wx, wy, wz, ge::voxel::BlockId::Air, 0u);
                return;
            }
        }

        BlockRayHit hit{};
        if (!RaycastSolidBlock(hit)) return;
        (void)SubmitBlockEdit(hit.wx, hit.wy, hit.wz, ge::voxel::BlockId::Air, 0u);
    }

    inline void PlaceSelectedBlock() noexcept {
        BlockRayHit hit{};
        if (!RaycastSolidBlock(hit)) {
            const ge::item::Stack& selected_stack = SelectedHotbarStack();
            if (!ge::item::is_empty(selected_stack) && ge::item::is_consumable(selected_stack.id))
                QueueUseSelectedItem();
            return;
        }
        const ge::voxel::BlockId selected = SelectedQuickSlotBlock();
        if (selected == ge::voxel::BlockId::Air) {
            const ge::item::Stack& selected_stack = SelectedHotbarStack();
            if (!ge::item::is_empty(selected_stack) && ge::item::is_consumable(selected_stack.id))
                QueueUseSelectedItem();
            return;
        }
        if (voxel_world.get_world_block(hit.prev_wx, hit.prev_wy, hit.prev_wz) != ge::voxel::BlockId::Air)
            return;
        if (IsBlockOnPlayerHeadLevel(hit.prev_wx, hit.prev_wy, hit.prev_wz))
            return;
        (void)SubmitBlockEdit(hit.prev_wx, hit.prev_wy, hit.prev_wz, selected, 0u);
    }
