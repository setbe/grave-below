    inline bool SetBlockFaceUv(ge::voxel::BlockId id, io::u8 face, io::char_view tex_name) noexcept {
        const io::u16 bid = ge::voxel::block_index(id);
        if (!block_face_uv) return false;
        if (bid >= ge::voxel::BLOCK_COUNT || face >= FACE_INDEX_COUNT) return false;
        return ResolveTextureUv(tex_name, BlockUvRef(bid, face));
    }

    inline void SetBlockUvAllFaces(ge::voxel::BlockId id, io::char_view tex_name) noexcept {
        for (io::u8 face = 0; face < FACE_INDEX_COUNT; ++face)
            (void)SetBlockFaceUv(id, face, tex_name);
    }

    inline void SetBlockUvGrassLike(ge::voxel::BlockId id) noexcept {
        // Face order in mesh_builder: +X, -X, +Y, -Y, +Z, -Z
        (void)SetBlockFaceUv(id, 0, "grass_side");
        (void)SetBlockFaceUv(id, 1, "grass_side");
        (void)SetBlockFaceUv(id, 2, "grass_up");
        (void)SetBlockFaceUv(id, 3, "dirt");
        (void)SetBlockFaceUv(id, 4, "grass_side");
        (void)SetBlockFaceUv(id, 5, "grass_side");
    }

    inline void SetBlockUvLogLike(ge::voxel::BlockId id) noexcept {
        // Face order in mesh_builder: +X, -X, +Y, -Y, +Z, -Z
        (void)SetBlockFaceUv(id, 0, "log-side");
        (void)SetBlockFaceUv(id, 1, "log-side");
        (void)SetBlockFaceUv(id, 2, "log");
        (void)SetBlockFaceUv(id, 3, "log");
        (void)SetBlockFaceUv(id, 4, "log-side");
        (void)SetBlockFaceUv(id, 5, "log-side");
    }

    inline void BuildBlockFaceUvTable() noexcept {
        if (!block_face_uv) return;
        for (io::u16 bid = 0; bid < ge::voxel::BLOCK_COUNT; ++bid)
            for (io::u8 face = 0; face < FACE_INDEX_COUNT; ++face)
                BlockUvRef(bid, face) = {};

        SetBlockUvAllFaces(ge::voxel::BlockId::Air, "grass_up");
        SetBlockUvGrassLike(ge::voxel::BlockId::Grass);
        SetBlockUvAllFaces(ge::voxel::BlockId::Dirt, "dirt");
        SetBlockUvAllFaces(ge::voxel::BlockId::Stone, "stone");
        SetBlockUvAllFaces(ge::voxel::BlockId::Sand, "sand");
        SetBlockUvAllFaces(ge::voxel::BlockId::Water, "water");
        (void)SetBlockFaceUv(ge::voxel::BlockId::Blood, 0, "blood");
        if (!BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::Blood), 0).valid)
            SetBlockUvAllFaces(ge::voxel::BlockId::Blood, "water");
        else
            SetBlockUvAllFaces(ge::voxel::BlockId::Blood, "blood");
        (void)SetBlockFaceUv(ge::voxel::BlockId::Slime, 0, "slime");
        if (!BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::Slime), 0).valid)
            SetBlockUvAllFaces(ge::voxel::BlockId::Slime, "water");
        else
            SetBlockUvAllFaces(ge::voxel::BlockId::Slime, "slime");
        SetBlockUvAllFaces(ge::voxel::BlockId::Snow, "snow");

        SetBlockUvGrassLike(ge::voxel::BlockId::GrassPale);
        SetBlockUvAllFaces(ge::voxel::BlockId::DirtDry, "dirt");
        SetBlockUvAllFaces(ge::voxel::BlockId::StoneCracked, "hardened_stone");
        SetBlockUvAllFaces(ge::voxel::BlockId::SandAsh, "sand");
        SetBlockUvAllFaces(ge::voxel::BlockId::WaterDark, "water");
        (void)SetBlockFaceUv(ge::voxel::BlockId::BloodDark, 0, "blood");
        if (!BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::BloodDark), 0).valid)
            SetBlockUvAllFaces(ge::voxel::BlockId::BloodDark, "water");
        else
            SetBlockUvAllFaces(ge::voxel::BlockId::BloodDark, "blood");
        (void)SetBlockFaceUv(ge::voxel::BlockId::SlimeDark, 0, "slime");
        if (!BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::SlimeDark), 0).valid)
            SetBlockUvAllFaces(ge::voxel::BlockId::SlimeDark, "water");
        else
            SetBlockUvAllFaces(ge::voxel::BlockId::SlimeDark, "slime");
        SetBlockUvAllFaces(ge::voxel::BlockId::SnowDirty, "snow");
        SetBlockUvLogLike(ge::voxel::BlockId::Log);
        SetBlockUvAllFaces(ge::voxel::BlockId::Leaves, "leaves");

        BlockFaceUv fallback{};
        if (!ResolveTextureUv("grass_up", fallback) && texture_atlas.texture_count > 0u) {
            float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
            if (ge::ResourceManager::texture_uv_of(texture_atlas, 0u, u0, v0, u1, v1)) {
                fallback.u0 = u0;
                fallback.v0 = v0;
                fallback.u1 = u1;
                fallback.v1 = v1;
                fallback.valid = true;
            }
        }

        if (!fallback.valid) return;

        for (io::u16 bid = 0; bid < ge::voxel::BLOCK_COUNT; ++bid)
            for (io::u8 face = 0; face < FACE_INDEX_COUNT; ++face)
                if (!BlockUvRef(bid, face).valid)
                    BlockUvRef(bid, face) = fallback;
    }

    IO_NODISCARD static inline float frac01(float value) noexcept {
        const io::i32 iv = floor_to_i32(value);
        return value - static_cast<float>(iv);
    }

    IO_NODISCARD inline BlockFaceUv FarMaterialUv(float h_avg, float slope) const noexcept {
        if (h_avg >= 92.f) return BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::Snow), 2u);
        if (slope >= 10.f) return BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::Stone), 2u);
        if (h_avg <= 24.f) return BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::Sand), 2u);
        return BlockUvRef(ge::voxel::block_index(ge::voxel::BlockId::Grass), 2u);
    }

    IO_NODISCARD static inline lm::vec3 TriangleNormal(const lm::vec3& a,
                                                       const lm::vec3& b,
                                                       const lm::vec3& c) noexcept {
        lm::vec3 n = lm::vec3_cross(b - a, c - a);
        const float len2 = lm::vec_dot(n, n);
        if (len2 <= 0.000001f) return { 0.f, 1.f, 0.f };
        return n * (1.f / lm::sqrtf(len2));
    }

    IO_NODISCARD inline bool PushFarVertex(io::vector<ge::voxel::MeshVertex>& out_vertices,
                                           io::vector<io::u32>& out_indices,
                                           const lm::vec3& p,
                                           const lm::vec3& n,
                                           float world_x, float world_z,
                                           const BlockFaceUv& uv_rect,
                                           io::u16 block_id) noexcept {
        ge::voxel::MeshVertex v{};
        v.px = p[0];
        v.py = p[1];
        v.pz = p[2];
        v.nx = n[0];
        v.ny = n[1];
        v.nz = n[2];
        const float tile_u = frac01(world_x * 0.125f);
        const float tile_v = frac01(world_z * 0.125f);
        v.u = uv_rect.u0 + (uv_rect.u1 - uv_rect.u0) * tile_u;
        v.v = uv_rect.v0 + (uv_rect.v1 - uv_rect.v0) * tile_v;
        v.atlas_u0 = uv_rect.u0;
        v.atlas_v0 = uv_rect.v0;
        v.atlas_u1 = uv_rect.u1;
        v.atlas_v1 = uv_rect.v1;
        v.block_id = block_id;
        v.face = 2u;
        v.ao = 255u;
        if (!out_vertices.push_back(v)) return false;
        if (!out_indices.push_back(static_cast<io::u32>(out_vertices.size() - 1u))) return false;
        return true;
    }

    IO_NODISCARD inline bool PushFarTriangle(io::vector<ge::voxel::MeshVertex>& out_vertices,
                                             io::vector<io::u32>& out_indices,
                                             const lm::vec3& a, const lm::vec3& b, const lm::vec3& c,
                                             float world_ax, float world_az,
                                             float world_bx, float world_bz,
                                             float world_cx, float world_cz,
                                             const BlockFaceUv& uv_rect,
                                             io::u16 block_id) noexcept {
        lm::vec3 n = TriangleNormal(a, b, c);
        if (n[1] < 0.f) n = n * -1.f;
        if (!PushFarVertex(out_vertices, out_indices, a, n, world_ax, world_az, uv_rect, block_id)) return false;
        if (!PushFarVertex(out_vertices, out_indices, b, n, world_bx, world_bz, uv_rect, block_id)) return false;
        if (!PushFarVertex(out_vertices, out_indices, c, n, world_cx, world_cz, uv_rect, block_id)) return false;
        return true;
    }


