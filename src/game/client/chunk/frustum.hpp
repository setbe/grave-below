#pragma once

    struct FrustumPlane {
        float a = 0.f;
        float b = 0.f;
        float c = 0.f;
        float d = 0.f;
    };

    struct Frustum {
        FrustumPlane planes[6]{};
    };

    static inline float absf(float value) noexcept {
        return value < 0.f ? -value : value;
    }

    static inline void normalize_plane(FrustumPlane& p) noexcept {
        const float len2 = p.a * p.a + p.b * p.b + p.c * p.c;
        if (len2 <= 0.000001f) return;
        const float inv = 1.f / lm::sqrtf(len2);
        p.a *= inv;
        p.b *= inv;
        p.c *= inv;
        p.d *= inv;
    }

    static inline void extract_frustum(const lm::mat4& vp, Frustum& out) noexcept {
        const float r0x = vp[0][0], r0y = vp[1][0], r0z = vp[2][0], r0w = vp[3][0];
        const float r1x = vp[0][1], r1y = vp[1][1], r1z = vp[2][1], r1w = vp[3][1];
        const float r2x = vp[0][2], r2y = vp[1][2], r2z = vp[2][2], r2w = vp[3][2];
        const float r3x = vp[0][3], r3y = vp[1][3], r3z = vp[2][3], r3w = vp[3][3];

        out.planes[0] = { r3x + r0x, r3y + r0y, r3z + r0z, r3w + r0w }; // left
        out.planes[1] = { r3x - r0x, r3y - r0y, r3z - r0z, r3w - r0w }; // right
        out.planes[2] = { r3x + r1x, r3y + r1y, r3z + r1z, r3w + r1w }; // bottom
        out.planes[3] = { r3x - r1x, r3y - r1y, r3z - r1z, r3w - r1w }; // top
        out.planes[4] = { r3x + r2x, r3y + r2y, r3z + r2z, r3w + r2w }; // near
        out.planes[5] = { r3x - r2x, r3y - r2y, r3z - r2z, r3w - r2w }; // far

        for (io::u32 i = 0; i < 6u; ++i)
            normalize_plane(out.planes[i]);
    }

    static inline bool frustum_intersects_chunk(const Frustum& fr, const ge::voxel::ChunkCoord& coord,
                                                const lm::vec3& camera_pos) noexcept {
        const float min_x = static_cast<float>(coord.x * static_cast<io::i32>(ge::voxel::CHUNK_SIZE)) - camera_pos[0];
        const float min_y = static_cast<float>(coord.y * static_cast<io::i32>(ge::voxel::CHUNK_SIZE)) - camera_pos[1];
        const float min_z = static_cast<float>(coord.z * static_cast<io::i32>(ge::voxel::CHUNK_SIZE)) - camera_pos[2];
        const float max_x = min_x + static_cast<float>(ge::voxel::CHUNK_SIZE);
        const float max_y = min_y + static_cast<float>(ge::voxel::CHUNK_SIZE);
        const float max_z = min_z + static_cast<float>(ge::voxel::CHUNK_SIZE);

        for (io::u32 i = 0; i < 6u; ++i) {
            const FrustumPlane& p = fr.planes[i];
            const float x = (p.a >= 0.f) ? max_x : min_x;
            const float y = (p.b >= 0.f) ? max_y : min_y;
            const float z = (p.c >= 0.f) ? max_z : min_z;
            if (p.a * x + p.b * y + p.c * z + p.d < 0.f)
                return false;
        }
        return true;
    }

    static inline bool frustum_intersects_aabb(const Frustum& fr,
                                               float min_x, float min_y, float min_z,
                                               float max_x, float max_y, float max_z) noexcept {
        for (io::u32 i = 0; i < 6u; ++i) {
            const FrustumPlane& p = fr.planes[i];
            const float x = (p.a >= 0.f) ? max_x : min_x;
            const float y = (p.b >= 0.f) ? max_y : min_y;
            const float z = (p.c >= 0.f) ? max_z : min_z;
            if (p.a * x + p.b * y + p.c * z + p.d < 0.f)
                return false;
        }
        return true;
    }

    static inline float chunk_distance2_to_camera(const ge::voxel::ChunkCoord& coord,
                                                  const lm::vec3& cam_pos) noexcept {
        const float half = static_cast<float>(ge::voxel::CHUNK_SIZE) * 0.5f;
        const float cx = static_cast<float>(coord.x * static_cast<io::i32>(ge::voxel::CHUNK_SIZE)) + half;
        const float cy = static_cast<float>(coord.y * static_cast<io::i32>(ge::voxel::CHUNK_SIZE)) + half;
        const float cz = static_cast<float>(coord.z * static_cast<io::i32>(ge::voxel::CHUNK_SIZE)) + half;
        const float dx = cx - cam_pos[0];
        const float dy = cy - cam_pos[1];
        const float dz = cz - cam_pos[2];
        return dx * dx + dy * dy + dz * dz;
    }
