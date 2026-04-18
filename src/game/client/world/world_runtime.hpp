    inline void ClearChunkWorld() noexcept {
        WaitChunkJobsIdle();
        chunk_world_ready = false;
        chunk_center_valid = false;
        ResetSandLerpVisuals();
        ResetWorldActors();
        voxel_world.clear();
        chunk_meshes.clear();
        transparent_chunk_draw.clear();
        ClearChunkSlotLookup();
        chunk_slot_marks.clear();
        chunk_requests_inflight.clear();
        ClearInflightLookup();
        InvalidateChunkRequestOffsets();
        chunk_request_virtual_count = 0u;
        chunk_schedule_cursor = 0;
        chunk_meshes_visible = 0;
        chunk_meshes_culled = 0;
    }

    inline bool InitChunkWorld() noexcept {
        ClearChunkWorld();

        const io::u32 requested_rd = ClampRenderDistance(render_distance_chunks);
        io::u32 chosen_rd = requested_rd;
        bool inited = false;

        while (chosen_rd >= 1u) {
            render_distance_chunks = chosen_rd;
            io::usize want = ComputeRequestVirtualCount();
            if (want == 0u) {
                if (chosen_rd == 1u) break;
                --chosen_rd;
                continue;
            }

            voxel_world.clear();
            voxel_world.max_chunks = want;
            chunk_meshes.clear();
            ClearChunkSlotLookup();
            chunk_slot_marks.clear();
            chunk_requests_inflight.clear();
            ClearInflightLookup();
            chunk_request_scan_cursor = 0u;
            chunk_request_virtual_count = want;
            InvalidateChunkRequestOffsets();

            if (!voxel_world.init(0u)) {
                if (chosen_rd == 1u) break;
                --chosen_rd;
                continue;
            }
            io::usize reserve_chunks = want;
            if (reserve_chunks > 12288u) reserve_chunks = 12288u;
            if (reserve_chunks < 512u) reserve_chunks = 512u;
            if (!voxel_world.chunks.reserve(reserve_chunks)) {
                if (chosen_rd == 1u) break;
                --chosen_rd;
                continue;
            }
            if (!chunk_meshes.reserve(reserve_chunks)) {
                if (chosen_rd == 1u) break;
                --chosen_rd;
                continue;
            }
            if (!transparent_chunk_draw.reserve(reserve_chunks)) {
                if (chosen_rd == 1u) break;
                --chosen_rd;
                continue;
            }
            voxel_world.max_chunks = reserve_chunks;
            if (!chunk_requests_inflight.reserve(192u)) {
                if (chosen_rd == 1u) break;
                --chosen_rd;
                continue;
            }
            if (!EnsureChunkRequestOffsets()) {
                if (chosen_rd == 1u) break;
                --chosen_rd;
                continue;
            }

            inited = true;
            break;
        }

        if (!inited) return false;

        render_distance_chunks = chosen_rd;
        render_distance_pending = static_cast<float>(chosen_rd);
        if (chosen_rd != requested_rd) {
            io::StackOut<128> msg{};
            msg << "Render distance lowered to " << chosen_rd << " due to memory limits";
            PushSystemChat(msg.view());
        }

        const ge::voxel::ChunkCoord center = CameraChunkCoord();
        RecenterChunkWorld(center, true);
        if (!chunk_center_valid) return false;

        chunk_world_ready = true;
        chunk_jobs_submitted = 0;
        chunk_jobs_completed = 0;
        chunk_jobs_failed = 0;
        chunk_faces_last = 0;
        chunk_vertices_last = 0;
        chunk_meshes_culled = 0;
        return true;
    }

    inline void UpdateChunkStreaming() noexcept {
        if (!chunk_world_ready)
            return;
        const ge::voxel::ChunkCoord current = CameraChunkCoord();
        if (chunk_center_valid && ge::voxel::coord_eq(current, chunk_center_coord))
            return;
        if (HasActiveChunkJobs()) {
            UpdateChunkRequestCenterOnly(current);
            const io::u32 sxz = render_distance_chunks * 2u + 1u;
            const io::u32 sy = static_cast<io::u32>(RequestYRadiusChunks() * 2 + 1);
            io::u32 prune_budget = sxz * sy; // one slab when crossing one chunk in X/Z
            prune_budget *= 2u; // diagonal / fast movement safety margin
            if (prune_budget < 128u) prune_budget = 128u;
            if (prune_budget > 2048u) prune_budget = 2048u;
            if (voxel_world.max_chunks > 0u && voxel_world.chunks.size() > voxel_world.max_chunks) {
                io::u32 overflow = static_cast<io::u32>(voxel_world.chunks.size() - voxel_world.max_chunks);
                if (overflow > 2048u) overflow = 2048u;
                prune_budget += overflow;
            }
            PruneChunkWorldIncremental(current, prune_budget);
            return;
        }

        RecenterChunkWorld(current, false);
    }

#include "../chunk/mesh_job.hpp"

    inline bool InitBlockCubeMesh(ge::voxel::BlockId id,
                                  gl::Buffer& out_vbo,
                                  gl::Buffer& out_ebo,
                                  gl::VertexArray& out_vao,
                                  io::u32& out_index_count) noexcept {
        ge::voxel::MeshVertex verts[24]{};
        io::u32 indices[36]{};
        ge::voxel::MeshBuffers mesh{};
        mesh.vertices = verts;
        mesh.vertex_capacity = 24u;
        mesh.indices = indices;
        mesh.index_capacity = 36u;
        mesh.reset_counts();

        using ge::voxel::Face;
        if (!ge::voxel::push_face(mesh, id, Face::PosX, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, id, Face::NegX, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, id, Face::PosY, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, id, Face::NegY, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, id, Face::PosZ, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, id, Face::NegZ, 0.f, 0.f, 0.f)) return false;

        const io::u16 bid = ge::voxel::block_index(id);
        for (io::u32 i = 0; i < mesh.vertex_count; ++i) {
            ge::voxel::MeshVertex& v = mesh.vertices[i];
            const io::u8 face = v.face < FACE_INDEX_COUNT ? v.face : 0u;
            const BlockFaceUv uv = BlockUvRef(bid, face);
            const float du = uv.u1 - uv.u0;
            const float dv = uv.v1 - uv.v0;
            // Menu cubes are drawn in non-tiled mode, so UV must already be atlas-space.
            v.u = uv.u0 + v.u * du;
            v.v = uv.v0 + v.v * dv;
            v.atlas_u0 = uv.u0;
            v.atlas_v0 = uv.v0;
            v.atlas_u1 = uv.u1;
            v.atlas_v1 = uv.v1;
        }

        out_vbo.bind();
        out_vbo.data(mesh.vertices,
                     static_cast<io::usize>(mesh.vertex_count) * sizeof(ge::voxel::MeshVertex),
                     gl::BufferUsage::StaticDraw);
        out_ebo.bind();
        out_ebo.data(mesh.indices,
                     static_cast<io::usize>(mesh.index_count) * sizeof(io::u32),
                     gl::BufferUsage::StaticDraw);

        out_vao.bind();
        gl::VertexAttribPointer(0, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(0));
        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(1, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 3u));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(2, 2, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 6u));
        gl::EnableVertexAttribArray(2);
        gl::VertexAttribPointer(3, 4, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 8u));
        gl::EnableVertexAttribArray(3);
        gl::VertexAttribPointer(4, 1, gl::DrawElementsType::UnsignedByte, true,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 12u + sizeof(io::u16) + sizeof(io::u8)));
        gl::EnableVertexAttribArray(4);
        gl::VertexAttribIPointer(5, 1, gl::DrawElementsType::UnsignedShort,
                                 static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                 reinterpret_cast<void*>(sizeof(float) * 12u));
        gl::EnableVertexAttribArray(5);
        gl::BindVertexArray(0);

        out_index_count = mesh.index_count;
        return out_index_count > 0u;
    }

    inline bool InitMenuBlockMeshes() noexcept {
        const bool grass_ok = InitBlockCubeMesh(ge::voxel::BlockId::Grass, vbo, ebo, vao, draw_index_count);
        const bool dirt_ok = InitBlockCubeMesh(ge::voxel::BlockId::Dirt,
                                               menu_dirt_vbo, menu_dirt_ebo, menu_dirt_vao, menu_dirt_index_count);
        const bool stone_ok = InitBlockCubeMesh(ge::voxel::BlockId::Stone,
                                                menu_stone_vbo, menu_stone_ebo, menu_stone_vao, menu_stone_index_count);
        return grass_ok && dirt_ok && stone_ok;
    }

    inline bool InitSandLerpMesh() noexcept {
        ge::voxel::MeshVertex verts[24]{};
        io::u32 indices[36]{};
        ge::voxel::MeshBuffers mesh{};
        mesh.vertices = verts;
        mesh.vertex_capacity = 24u;
        mesh.indices = indices;
        mesh.index_capacity = 36u;
        mesh.reset_counts();

        using ge::voxel::Face;
        if (!ge::voxel::push_face(mesh, ge::voxel::BlockId::Sand, Face::PosX, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, ge::voxel::BlockId::Sand, Face::NegX, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, ge::voxel::BlockId::Sand, Face::PosY, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, ge::voxel::BlockId::Sand, Face::NegY, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, ge::voxel::BlockId::Sand, Face::PosZ, 0.f, 0.f, 0.f)) return false;
        if (!ge::voxel::push_face(mesh, ge::voxel::BlockId::Sand, Face::NegZ, 0.f, 0.f, 0.f)) return false;

        const io::u16 sand_id = ge::voxel::block_index(ge::voxel::BlockId::Sand);
        for (io::u32 i = 0; i < mesh.vertex_count; ++i) {
            ge::voxel::MeshVertex& v = mesh.vertices[i];
            const io::u8 face = v.face < FACE_INDEX_COUNT ? v.face : 0u;
            const BlockFaceUv uv = BlockUvRef(sand_id, face);
            v.atlas_u0 = uv.u0;
            v.atlas_v0 = uv.v0;
            v.atlas_u1 = uv.u1;
            v.atlas_v1 = uv.v1;
        }

        sand_lerp_vbo.bind();
        sand_lerp_vbo.data(mesh.vertices,
                           static_cast<io::usize>(mesh.vertex_count) * sizeof(ge::voxel::MeshVertex),
                           gl::BufferUsage::StaticDraw);
        sand_lerp_ebo.bind();
        sand_lerp_ebo.data(mesh.indices,
                           static_cast<io::usize>(mesh.index_count) * sizeof(io::u32),
                           gl::BufferUsage::StaticDraw);

        sand_lerp_vao.bind();
        gl::VertexAttribPointer(0, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(0));
        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(1, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 3u));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(2, 2, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 6u));
        gl::EnableVertexAttribArray(2);
        gl::VertexAttribPointer(3, 4, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 8u));
        gl::EnableVertexAttribArray(3);
        gl::VertexAttribPointer(4, 1, gl::DrawElementsType::UnsignedByte, true,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 12u + sizeof(io::u16) + sizeof(io::u8)));
        gl::EnableVertexAttribArray(4);
        gl::VertexAttribIPointer(5, 1, gl::DrawElementsType::UnsignedShort,
                                 static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                 reinterpret_cast<void*>(sizeof(float) * 12u));
        gl::EnableVertexAttribArray(5);
        gl::BindVertexArray(0);

        sand_lerp_index_count = mesh.index_count;
        return sand_lerp_index_count > 0u;
    }

    inline bool InitItemSpriteMesh() noexcept {
        ge::voxel::MeshVertex verts[4]{};
        const io::u32 indices[6]{ 0u, 1u, 2u, 0u, 2u, 3u };
        for (io::u32 i = 0u; i < 4u; ++i) {
            verts[i].nx = 0.f;
            verts[i].ny = 0.f;
            verts[i].nz = 1.f;
            verts[i].block_id = ge::voxel::block_index(ge::voxel::BlockId::Grass);
            verts[i].face = 4u;
            verts[i].ao = 255u;
        }

        item_sprite_vbo.bind();
        item_sprite_vbo.data(verts, sizeof(verts), gl::BufferUsage::DynamicDraw);
        item_sprite_ebo.bind();
        item_sprite_ebo.data(indices, sizeof(indices), gl::BufferUsage::StaticDraw);

        item_sprite_vao.bind();
        gl::VertexAttribPointer(0, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(0));
        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(1, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 3u));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(2, 2, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 6u));
        gl::EnableVertexAttribArray(2);
        gl::VertexAttribPointer(3, 4, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 8u));
        gl::EnableVertexAttribArray(3);
        gl::VertexAttribPointer(4, 1, gl::DrawElementsType::UnsignedByte, true,
                                static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 12u + sizeof(io::u16) + sizeof(io::u8)));
        gl::EnableVertexAttribArray(4);
        gl::VertexAttribIPointer(5, 1, gl::DrawElementsType::UnsignedShort,
                                 static_cast<int>(sizeof(ge::voxel::MeshVertex)),
                                 reinterpret_cast<void*>(sizeof(float) * 12u));
        gl::EnableVertexAttribArray(5);
        gl::BindVertexArray(0);

        item_sprite_index_count = 6u;
        return true;
    }

    inline bool InitRegionLineMesh() noexcept {
        const lm::vec3 seed_lines[2]{
            lm::vec3{ 0.f, 0.f, 0.f },
            lm::vec3{ 0.f, 1.f, 0.f }
        };
        region_line_vbo.bind();
        region_line_vbo.data(seed_lines, sizeof(seed_lines), gl::BufferUsage::DynamicDraw);

        region_line_vao.bind();
        gl::VertexAttribPointer(0, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(lm::vec3)),
                                reinterpret_cast<void*>(0));
        gl::EnableVertexAttribArray(0);
        gl::BindVertexArray(0);
        region_line_vertex_count = 2u;
        return true;
    }

    inline void DrawChunkWorld(const lm::mat4& view, const lm::mat4& proj,
                               const lm::vec3& camera_world, io::u8 render_mask,
                               int u_model_location) noexcept {
        const bool track_metrics = (render_mask == CHUNK_RENDER_MASK_SOLID);
        if (track_metrics) {
            chunk_meshes_visible = 0;
            chunk_meshes_culled = 0;
        }
        Frustum fr{};
        const lm::mat4 vp = proj * view;
        extract_frustum(vp, fr);

        if (!track_metrics) {
            transparent_chunk_draw.clear();
            for (io::usize i = 0; i < chunk_meshes.size(); ++i) {
                const ChunkRenderMesh& chunk = chunk_meshes[i];
                if (!chunk.uploaded || chunk.index_count == 0) continue;
                if ((chunk.render_mask & render_mask) == 0u) continue;
                if (!frustum_intersects_chunk(fr, chunk.coord, camera_world)) continue;
                TransparentChunkDrawItem item{};
                item.chunk_index = i;
                item.distance2 = chunk_distance2_to_camera(chunk.coord, camera_world);
                if (!transparent_chunk_draw.push_back(item))
                    break;
            }

            for (io::usize i = 1; i < transparent_chunk_draw.size(); ++i) {
                TransparentChunkDrawItem key = transparent_chunk_draw[i];
                io::usize j = i;
                while (j > 0 && transparent_chunk_draw[j - 1].distance2 < key.distance2) {
                    transparent_chunk_draw[j] = transparent_chunk_draw[j - 1];
                    --j;
                }
                transparent_chunk_draw[j] = key;
            }

            for (io::usize i = 0; i < transparent_chunk_draw.size(); ++i) {
                const io::usize chunk_index = transparent_chunk_draw[i].chunk_index;
                if (chunk_index >= chunk_meshes.size()) continue;
                const ChunkRenderMesh& chunk = chunk_meshes[chunk_index];
                io::i32 ox = 0, oy = 0, oz = 0;
                ge::voxel::chunk_origin_world(chunk.coord, ox, oy, oz);
                const float rel_x = static_cast<float>(ox) - camera_world[0];
                const float rel_y = static_cast<float>(oy) - camera_world[1];
                const float rel_z = static_cast<float>(oz) - camera_world[2];
                const lm::mat4 model = lm::mat4_translate(rel_x, rel_y, rel_z);
                if (u_model_location >= 0)
                    gl::UniformMatrix4fv(u_model_location, 1, false, model[0].data());

                chunk.vao.bind();
                chunk.ebo.bind();
                gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(chunk.index_count),
                                 gl::DrawElementsType::UnsignedInt, nullptr);
            }
            return;
        }

        for (io::usize i = 0; i < chunk_meshes.size(); ++i) {
            const ChunkRenderMesh& chunk = chunk_meshes[i];
            if (!chunk.uploaded || chunk.index_count == 0) continue;
            if ((chunk.render_mask & render_mask) == 0u) continue;
            if (!frustum_intersects_chunk(fr, chunk.coord, camera_world)) {
                if (track_metrics)
                    ++chunk_meshes_culled;
                continue;
            }

            io::i32 ox = 0, oy = 0, oz = 0;
            ge::voxel::chunk_origin_world(chunk.coord, ox, oy, oz);
            const float rel_x = static_cast<float>(ox) - camera_world[0];
            const float rel_y = static_cast<float>(oy) - camera_world[1];
            const float rel_z = static_cast<float>(oz) - camera_world[2];
            const lm::mat4 model = lm::mat4_translate(rel_x, rel_y, rel_z);
            if (u_model_location >= 0)
                gl::UniformMatrix4fv(u_model_location, 1, false, model[0].data());

            chunk.vao.bind();
            chunk.ebo.bind();
            gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(chunk.index_count),
                             gl::DrawElementsType::UnsignedInt, nullptr);
            if (track_metrics)
                ++chunk_meshes_visible;
        }
    }

    inline void DrawFarTerrain(const lm::mat4& view, const lm::mat4& proj, const lm::vec3& camera_world) noexcept {
        (void)view;
        (void)proj;
        (void)camera_world;
    }

    inline void DrawSandLerpVisuals(io::u64 now_ms, const lm::vec3& camera_world) noexcept {
        if (!sand_lerp_visuals) return;
        if (sand_lerp_index_count == 0u) return;
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 1);

        sand_lerp_vao.bind();
        sand_lerp_ebo.bind();
        for (io::u32 i = 0; i < SAND_LERP_VISUAL_CAP; ++i) {
            SandLerpVisual& v = sand_lerp_visuals[i];
            if (!v.active) continue;
            io::u32 elapsed_ms = 0u;
            if (now_ms >= v.start_ms) {
                const io::u64 delta = now_ms - v.start_ms;
                elapsed_ms = (delta > 0xFFFFFFFFull) ? 0xFFFFFFFFu : static_cast<io::u32>(delta);
            }
            if (elapsed_ms >= v.duration_ms) {
                v.active = false;
                continue;
            }
            float t = static_cast<float>(elapsed_ms) / static_cast<float>(v.duration_ms);
            t = clampf(t, 0.f, 1.f);
            t = t * t * (3.f - 2.f * t);
            const float x = v.src_x + (v.dst_x - v.src_x) * t - camera_world[0];
            const float y = v.src_y + (v.dst_y - v.src_y) * t - camera_world[1];
            const float z = v.src_z + (v.dst_z - v.src_z) * t - camera_world[2];
            const lm::mat4 model = lm::mat4_translate(x, y, z);
            if (terrain_uniforms.u_model >= 0)
                gl::UniformMatrix4fv(terrain_uniforms.u_model, 1, false, model[0].data());
            gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(sand_lerp_index_count),
                             gl::DrawElementsType::UnsignedInt, nullptr);
        }
    }

    inline void DrawCube(const lm::mat4& model) noexcept {
        if (draw_index_count == 0) return;
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 0);
        if (terrain_uniforms.u_model >= 0) gl::UniformMatrix4fv(terrain_uniforms.u_model, 1, false, model[0].data());
        vao.bind();
        ebo.bind();
        gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(draw_index_count),
                         gl::DrawElementsType::UnsignedInt, nullptr);
    }

    inline void DrawMenuCube(ge::voxel::BlockId id, const lm::mat4& model) noexcept {
        gl::VertexArray* draw_vao = &vao;
        gl::Buffer* draw_ebo = &ebo;
        io::u32 index_count = draw_index_count;
        if (id == ge::voxel::BlockId::Dirt) {
            draw_vao = &menu_dirt_vao;
            draw_ebo = &menu_dirt_ebo;
            index_count = menu_dirt_index_count;
        } else if (id == ge::voxel::BlockId::Stone) {
            draw_vao = &menu_stone_vao;
            draw_ebo = &menu_stone_ebo;
            index_count = menu_stone_index_count;
        }
        if (index_count == 0u) return;
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 0);
        if (terrain_uniforms.u_model >= 0)
            gl::UniformMatrix4fv(terrain_uniforms.u_model, 1, false, model[0].data());
        draw_vao->bind();
        draw_ebo->bind();
        gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(index_count),
                         gl::DrawElementsType::UnsignedInt, nullptr);
    }

