#pragma once

    IO_NODISCARD inline bool TryGetWorldBlockCell(io::i32 wx, io::i32 wy, io::i32 wz,
                                                  ge::voxel::BlockId& out_id) const noexcept {
        ge::voxel::ChunkCoord cc{};
        io::u32 lx = 0, ly = 0, lz = 0;
        ge::voxel::split_world_coord(wx, wy, wz, cc, lx, ly, lz);
        const ge::voxel::ChunkData* chunk = voxel_world.find_chunk(cc);
        if (!chunk || !chunk->generated) return false;
        out_id = chunk->get(lx, ly, lz);
        return true;
    }

    IO_NODISCARD inline bool IsSolidWorldCell(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
        ge::voxel::BlockId id = ge::voxel::BlockId::Air;
        if (!TryGetWorldBlockCell(wx, wy, wz, id))
            return true; // treat missing chunk as solid to prevent falling through not-yet-loaded terrain
        return ge::voxel::is_solid(id);
    }

    IO_NODISCARD static inline float PlayerStandHeight() noexcept { return 1.80f; }
    IO_NODISCARD static inline float PlayerStandEyeToFeet() noexcept { return 1.62f; }
    IO_NODISCARD static inline float PlayerCrawlHeight() noexcept { return 1.00f; }
    IO_NODISCARD static inline float PlayerCrawlEyeToFeet() noexcept { return 1.00f; }

    IO_NODISCARD inline bool IsPlayerBodyCollidingAtForMode(const lm::vec3& eye_pos, bool crawling_mode) const noexcept {
        if (!chunk_world_ready) return false;

        static constexpr float PLAYER_RADIUS = 0.32f;
        static constexpr float EPS = 0.001f;
        const float player_height = crawling_mode ? PlayerCrawlHeight() : PlayerStandHeight();
        const float player_eye_to_feet = crawling_mode ? PlayerCrawlEyeToFeet() : PlayerStandEyeToFeet();

        const float min_x = eye_pos[0] - PLAYER_RADIUS;
        const float max_x = eye_pos[0] + PLAYER_RADIUS;
        const float min_y = eye_pos[1] - player_eye_to_feet;
        const float max_y = min_y + player_height;
        const float min_z = eye_pos[2] - PLAYER_RADIUS;
        const float max_z = eye_pos[2] + PLAYER_RADIUS;

        const io::i32 x0 = floor_to_i32(min_x + EPS);
        const io::i32 x1 = floor_to_i32(max_x - EPS);
        const io::i32 y0 = floor_to_i32(min_y + EPS);
        const io::i32 y1 = floor_to_i32(max_y - EPS);
        const io::i32 z0 = floor_to_i32(min_z + EPS);
        const io::i32 z1 = floor_to_i32(max_z - EPS);

        for (io::i32 y = y0; y <= y1; ++y)
            for (io::i32 z = z0; z <= z1; ++z)
                for (io::i32 x = x0; x <= x1; ++x)
                    if (IsSolidWorldCell(x, y, z))
                        return true;
        return false;
    }

    IO_NODISCARD inline bool IsPlayerBodyCollidingAt(const lm::vec3& eye_pos) const noexcept {
        return IsPlayerBodyCollidingAtForMode(eye_pos, player_crawling);
    }

    IO_NODISCARD inline bool TryGetHeadCollisionBlockAt(const lm::vec3& eye_pos,
                                                        ge::voxel::BlockId& out_block) const noexcept {
        if (!chunk_world_ready) return false;

        // Trigger anti-Xray only when eye/head is truly inside solid volume.
        // A light ceiling touch should not instantly fade to gray.
        static constexpr float PROBE_RING = 0.07f;
        static constexpr float PROBE_Y = 0.05f;

        io::u32 hits = 0u;
        ge::voxel::BlockId hit_block = ge::voxel::BlockId::Air;
        const float sx[5] = { 0.f,  PROBE_RING, -PROBE_RING, 0.f,        0.f };
        const float sy[5] = { PROBE_Y, PROBE_Y, PROBE_Y,     PROBE_Y,    PROBE_Y };
        const float sz[5] = { 0.f,  0.f,        0.f,         PROBE_RING, -PROBE_RING };

        for (io::u32 i = 0; i < 5u; ++i) {
            const io::i32 wx = floor_to_i32(eye_pos[0] + sx[i]);
            const io::i32 wy = floor_to_i32(eye_pos[1] + sy[i]);
            const io::i32 wz = floor_to_i32(eye_pos[2] + sz[i]);
            ge::voxel::BlockId cell = ge::voxel::BlockId::Air;
            if (!TryGetWorldBlockCell(wx, wy, wz, cell) || !ge::voxel::is_solid(cell))
                continue;
            ++hits;
            hit_block = cell;
            if (hits >= 2u) {
                out_block = hit_block;
                return true;
            }
        }
        return false;
    }

    IO_NODISCARD inline bool IsPlayerHeadCollidingAt(const lm::vec3& eye_pos) const noexcept {
        ge::voxel::BlockId block = ge::voxel::BlockId::Air;
        return TryGetHeadCollisionBlockAt(eye_pos, block);
    }

    IO_NODISCARD inline float PlayerFeetYForMode(const lm::vec3& eye_pos, bool crawling_mode) const noexcept {
        return eye_pos[1] - (crawling_mode ? PlayerCrawlEyeToFeet() : PlayerStandEyeToFeet());
    }

    IO_NODISCARD inline float PlayerFeetY(const lm::vec3& eye_pos) const noexcept {
        return PlayerFeetYForMode(eye_pos, player_crawling);
    }

    IO_NODISCARD inline bool IsPlayerGroundedAtForMode(const lm::vec3& eye_pos, bool crawling_mode) const noexcept {
        if (!chunk_world_ready) return false;
        static constexpr float PLAYER_RADIUS = 0.32f;
        static constexpr float GROUND_PROBE = 0.10f;
        static constexpr float EPS = 0.001f;

        const float min_x = eye_pos[0] - PLAYER_RADIUS;
        const float max_x = eye_pos[0] + PLAYER_RADIUS;
        const float feet_y = PlayerFeetYForMode(eye_pos, crawling_mode);
        const float min_z = eye_pos[2] - PLAYER_RADIUS;
        const float max_z = eye_pos[2] + PLAYER_RADIUS;

        const io::i32 x0 = floor_to_i32(min_x + EPS);
        const io::i32 x1 = floor_to_i32(max_x - EPS);
        const io::i32 y = floor_to_i32(feet_y - GROUND_PROBE);
        const io::i32 z0 = floor_to_i32(min_z + EPS);
        const io::i32 z1 = floor_to_i32(max_z - EPS);

        for (io::i32 z = z0; z <= z1; ++z)
            for (io::i32 x = x0; x <= x1; ++x)
                if (IsSolidWorldCell(x, y, z))
                    return true;
        return false;
    }

    IO_NODISCARD inline bool IsPlayerGroundedAt(const lm::vec3& eye_pos) const noexcept {
        return IsPlayerGroundedAtForMode(eye_pos, player_crawling);
    }
