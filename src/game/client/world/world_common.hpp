
    inline void UseTerrainPass(const lm::mat4& view, const lm::mat4& proj,
                             const lm::vec3& light_pos, const lm::vec3& view_pos,
                             const lm::vec3& sun_dir, float daylight, const lm::vec3& fog_color) noexcept {
        terrain_shader.Use();
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        if (terrain_uniforms.u_atlas >= 0) gl::Uniform1i(terrain_uniforms.u_atlas, 0);
        if (terrain_uniforms.u_atlas_texel >= 0) {
            const float inv_w = texture_atlas.atlas_width > 0 ? (1.f / static_cast<float>(texture_atlas.atlas_width)) : 0.f;
            const float inv_h = texture_atlas.atlas_height > 0 ? (1.f / static_cast<float>(texture_atlas.atlas_height)) : 0.f;
            gl::Uniform2f(terrain_uniforms.u_atlas_texel, inv_w, inv_h);
        }
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 1);
        if (terrain_uniforms.u_view >= 0) gl::UniformMatrix4fv(terrain_uniforms.u_view, 1, false, view[0].data());
        if (terrain_uniforms.u_proj >= 0) gl::UniformMatrix4fv(terrain_uniforms.u_proj, 1, false, proj[0].data());

        if (terrain_uniforms.u_light_x >= 0) gl::Uniform1f(terrain_uniforms.u_light_x, light_pos[0]);
        if (terrain_uniforms.u_light_y >= 0) gl::Uniform1f(terrain_uniforms.u_light_y, light_pos[1]);
        if (terrain_uniforms.u_light_z >= 0) gl::Uniform1f(terrain_uniforms.u_light_z, light_pos[2]);

        if (terrain_uniforms.u_view_x >= 0) gl::Uniform1f(terrain_uniforms.u_view_x, view_pos[0]);
        if (terrain_uniforms.u_view_y >= 0) gl::Uniform1f(terrain_uniforms.u_view_y, view_pos[1]);
        if (terrain_uniforms.u_view_z >= 0) gl::Uniform1f(terrain_uniforms.u_view_z, view_pos[2]);
        if (terrain_uniforms.u_sun_dir_x >= 0) gl::Uniform1f(terrain_uniforms.u_sun_dir_x, sun_dir[0]);
        if (terrain_uniforms.u_sun_dir_y >= 0) gl::Uniform1f(terrain_uniforms.u_sun_dir_y, sun_dir[1]);
        if (terrain_uniforms.u_sun_dir_z >= 0) gl::Uniform1f(terrain_uniforms.u_sun_dir_z, sun_dir[2]);
        if (terrain_uniforms.u_daylight >= 0) gl::Uniform1f(terrain_uniforms.u_daylight, daylight);
        if (terrain_uniforms.u_fog_r >= 0) gl::Uniform1f(terrain_uniforms.u_fog_r, fog_color[0]);
        if (terrain_uniforms.u_fog_g >= 0) gl::Uniform1f(terrain_uniforms.u_fog_g, fog_color[1]);
        if (terrain_uniforms.u_fog_b >= 0) gl::Uniform1f(terrain_uniforms.u_fog_b, fog_color[2]);
        const float base_vis = static_cast<float>(render_distance_chunks * ge::voxel::CHUNK_SIZE);
        const float day_scale = 0.9f + 0.2f * daylight;
        float fog_start = base_vis * 0.40f * day_scale;
        if (fog_start < 64.f) fog_start = 64.f;
        float fog_end = base_vis * 0.95f * day_scale + 20.f;
        if (fog_end < fog_start + 48.f) fog_end = fog_start + 48.f;
        if (terrain_uniforms.u_fog_start >= 0) gl::Uniform1f(terrain_uniforms.u_fog_start, fog_start);
        if (terrain_uniforms.u_fog_end >= 0) gl::Uniform1f(terrain_uniforms.u_fog_end, fog_end);
    }

    inline void UseLiquidPass(gl::Shader& shader, const LiquidUniforms& uniforms,
                              const lm::mat4& view, const lm::mat4& proj,
                              const lm::vec3& view_pos, const lm::vec3& sun_dir,
                              float daylight, const lm::vec3& fog_color,
                              float base_alpha, float fresnel_power,
                              float fresnel_strength, float edge_softness,
                              float edge_strength, int oit_pass) noexcept {
        shader.Use();
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        if (uniforms.u_atlas >= 0) gl::Uniform1i(uniforms.u_atlas, 0);
        if (uniforms.u_atlas_texel >= 0) {
            const float inv_w = texture_atlas.atlas_width > 0 ? (1.f / static_cast<float>(texture_atlas.atlas_width)) : 0.f;
            const float inv_h = texture_atlas.atlas_height > 0 ? (1.f / static_cast<float>(texture_atlas.atlas_height)) : 0.f;
            gl::Uniform2f(uniforms.u_atlas_texel, inv_w, inv_h);
        }
        if (uniforms.u_view >= 0) gl::UniformMatrix4fv(uniforms.u_view, 1, false, view[0].data());
        if (uniforms.u_proj >= 0) gl::UniformMatrix4fv(uniforms.u_proj, 1, false, proj[0].data());
        if (uniforms.u_view_x >= 0) gl::Uniform1f(uniforms.u_view_x, view_pos[0]);
        if (uniforms.u_view_y >= 0) gl::Uniform1f(uniforms.u_view_y, view_pos[1]);
        if (uniforms.u_view_z >= 0) gl::Uniform1f(uniforms.u_view_z, view_pos[2]);
        if (uniforms.u_sun_dir_x >= 0) gl::Uniform1f(uniforms.u_sun_dir_x, sun_dir[0]);
        if (uniforms.u_sun_dir_y >= 0) gl::Uniform1f(uniforms.u_sun_dir_y, sun_dir[1]);
        if (uniforms.u_sun_dir_z >= 0) gl::Uniform1f(uniforms.u_sun_dir_z, sun_dir[2]);
        if (uniforms.u_daylight >= 0) gl::Uniform1f(uniforms.u_daylight, daylight);
        if (uniforms.u_fog_r >= 0) gl::Uniform1f(uniforms.u_fog_r, fog_color[0]);
        if (uniforms.u_fog_g >= 0) gl::Uniform1f(uniforms.u_fog_g, fog_color[1]);
        if (uniforms.u_fog_b >= 0) gl::Uniform1f(uniforms.u_fog_b, fog_color[2]);
        const float base_vis = static_cast<float>(render_distance_chunks * ge::voxel::CHUNK_SIZE);
        const float day_scale = 0.9f + 0.2f * daylight;
        float fog_start = base_vis * 0.40f * day_scale;
        if (fog_start < 64.f) fog_start = 64.f;
        float fog_end = base_vis * 0.95f * day_scale + 20.f;
        if (fog_end < fog_start + 48.f) fog_end = fog_start + 48.f;
        if (uniforms.u_fog_start >= 0) gl::Uniform1f(uniforms.u_fog_start, fog_start);
        if (uniforms.u_fog_end >= 0) gl::Uniform1f(uniforms.u_fog_end, fog_end);
        if (uniforms.u_base_alpha >= 0) gl::Uniform1f(uniforms.u_base_alpha, base_alpha);
        if (uniforms.u_fresnel_power >= 0) gl::Uniform1f(uniforms.u_fresnel_power, fresnel_power);
        if (uniforms.u_fresnel_strength >= 0) gl::Uniform1f(uniforms.u_fresnel_strength, fresnel_strength);
        if (uniforms.u_edge_softness >= 0) gl::Uniform1f(uniforms.u_edge_softness, edge_softness);
        if (uniforms.u_edge_strength >= 0) gl::Uniform1f(uniforms.u_edge_strength, edge_strength);
        if (uniforms.u_oit_pass >= 0) gl::Uniform1i(uniforms.u_oit_pass, oit_pass);
    }

    inline void UseEntityPass(const lm::mat4& view, const lm::mat4& proj,
                              const lm::vec3& light_pos, const lm::vec3& view_pos,
                              float daylight, const lm::vec3& fog_color) noexcept {
        entity_shader.Use();
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        if (entity_uniforms.u_atlas >= 0) gl::Uniform1i(entity_uniforms.u_atlas, 0);
        if (entity_uniforms.u_view >= 0) gl::UniformMatrix4fv(entity_uniforms.u_view, 1, false, view[0].data());
        if (entity_uniforms.u_proj >= 0) gl::UniformMatrix4fv(entity_uniforms.u_proj, 1, false, proj[0].data());

        if (entity_uniforms.u_light_x >= 0) gl::Uniform1f(entity_uniforms.u_light_x, light_pos[0]);
        if (entity_uniforms.u_light_y >= 0) gl::Uniform1f(entity_uniforms.u_light_y, light_pos[1]);
        if (entity_uniforms.u_light_z >= 0) gl::Uniform1f(entity_uniforms.u_light_z, light_pos[2]);

        if (entity_uniforms.u_view_x >= 0) gl::Uniform1f(entity_uniforms.u_view_x, view_pos[0]);
        if (entity_uniforms.u_view_y >= 0) gl::Uniform1f(entity_uniforms.u_view_y, view_pos[1]);
        if (entity_uniforms.u_view_z >= 0) gl::Uniform1f(entity_uniforms.u_view_z, view_pos[2]);
        if (entity_uniforms.u_daylight >= 0) gl::Uniform1f(entity_uniforms.u_daylight, daylight);
        if (entity_uniforms.u_fog_r >= 0) gl::Uniform1f(entity_uniforms.u_fog_r, fog_color[0]);
        if (entity_uniforms.u_fog_g >= 0) gl::Uniform1f(entity_uniforms.u_fog_g, fog_color[1]);
        if (entity_uniforms.u_fog_b >= 0) gl::Uniform1f(entity_uniforms.u_fog_b, fog_color[2]);

        if (entity_uniforms.u_bones0 >= 0 && entity_bone_count > 0u)
            gl::UniformMatrix4fv(entity_uniforms.u_bones0, static_cast<int>(entity_bone_count), false, entity_bones[0][0].data());
    }

    static void ChunkBuildTaskEntry(void* arg) noexcept {
        ChunkMeshTaskArg* task = reinterpret_cast<ChunkMeshTaskArg*>(arg);
        if (!task || !task->owner) return;
        Window* self = reinterpret_cast<Window*>(task->owner);
        self->RunChunkBuildTask(task->slot_index);
    }

    inline bool ResolveTextureUv(io::char_view tex_name, BlockFaceUv& out_uv) noexcept {
        const io::u32 tex_id = ge::ResourceManager::texture_id_of(texture_atlas, tex_name);
        if (tex_id == ge::ResourceManager::INVALID_ID) return false;

        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        if (!ge::ResourceManager::texture_uv_of(texture_atlas, tex_id, u0, v0, u1, v1)) return false;
        out_uv.u0 = u0; out_uv.v0 = v0; out_uv.u1 = u1; out_uv.v1 = v1; out_uv.valid = true;
        return true;
    }


    static inline io::i32 floor_to_i32(float value) noexcept {
        io::i32 out = static_cast<io::i32>(value);
        if (static_cast<float>(out) > value) --out;
        return out;
    }

    IO_NODISCARD static inline io::i32 floor_div_i32(io::i32 a, io::i32 b) noexcept {
        io::i32 q = a / b;
        const io::i32 r = a % b;
        if (r != 0 && ((r > 0) != (b > 0))) --q;
        return q;
    }

    IO_NODISCARD static inline io::i32 quantize_chunk_coord(io::i32 value, io::i32 step) noexcept {
        return floor_div_i32(value, step) * step;
    }

    IO_NODISCARD static inline ge::voxel::ChunkCoord QuantizedFarCenter(const ge::voxel::ChunkCoord& c) noexcept {
        static constexpr io::i32 FAR_CENTER_STEP = 4;
        return ge::voxel::ChunkCoord{
            quantize_chunk_coord(c.x, FAR_CENTER_STEP),
            0,
            quantize_chunk_coord(c.z, FAR_CENTER_STEP)
        };
    }

    inline ge::voxel::ChunkCoord CameraChunkCoord() const noexcept {
        const io::i32 wx = floor_to_i32(camera.position[0]);
        const io::i32 wy = floor_to_i32(camera.position[1]);
        const io::i32 wz = floor_to_i32(camera.position[2]);
        ge::voxel::ChunkCoord out{};
        io::u32 lx = 0, ly = 0, lz = 0;
        ge::voxel::split_world_coord(wx, wy, wz, out, lx, ly, lz);
        return out;
    }

    IO_NODISCARD inline io::i32 WorldYRadiusChunks() const noexcept {
        io::i32 ry = 4;
        if (render_distance_chunks >= 12u) ry = 5;
        if (render_distance_chunks >= 24u) ry = 6;
        if (ry < WORLD_Y_RADIUS_CHUNKS) ry = WORLD_Y_RADIUS_CHUNKS;

        const io::u32 sx = static_cast<io::u32>(static_cast<io::i32>(render_distance_chunks) * 2 + 1);
        const io::u32 sz = sx;
        io::usize plane = 0u;
        if (voxel_world.max_chunks > 0u &&
            TryMulUsize(static_cast<io::usize>(sx), static_cast<io::usize>(sz), plane) &&
            plane > 0u) {
            io::usize max_slices = voxel_world.max_chunks / plane;
            if (max_slices < 1u) max_slices = 1u;
            io::i32 fit_ry = static_cast<io::i32>((max_slices - 1u) / 2u);
            if (fit_ry < 0) fit_ry = 0;
            if (ry > fit_ry) ry = fit_ry;
        }
        return ry;
    }

    IO_NODISCARD inline io::i32 RequestYRadiusChunks() const noexcept {
        return WorldYRadiusChunks();
    }

    IO_NODISCARD inline io::u32 ExtraRadiusChunks() const noexcept {
        io::u32 extra = extra_radius;
        if (extra < 1u) extra = 1u;
        if (extra > 16u) extra = 16u;
        return extra * 4u;
    }

    IO_NODISCARD inline float ChunkRequestPriority(const ge::voxel::ChunkCoord& coord,
                                                   const ge::voxel::ChunkCoord& center,
                                                   const lm::vec3& velocity) const noexcept {
        const float ox = static_cast<float>(coord.x - center.x);
        const float oy = static_cast<float>(coord.y - center.y);
        const float oz = static_cast<float>(coord.z - center.z);
        float score = ox * ox + oy * oy + oz * oz;

        const float speed2 = lm::vec_dot(velocity, velocity);
        if (speed2 > 0.00001f) {
            const float inv_speed = 1.f / lm::sqrtf(speed2);
            const float dir_x = velocity[0] * inv_speed;
            const float dir_y = velocity[1] * inv_speed;
            const float dir_z = velocity[2] * inv_speed;
            const float ahead = ox * dir_x + oy * dir_y + oz * dir_z;
            if (ahead > 0.f)
                score -= ahead * (0.8f + lm::sqrtf(speed2) * 0.12f);
        }
        return score;
    }

    inline ge::voxel::ChunkCoord ChunkCoordAtGridIndex(io::usize index,
                                                        const ge::voxel::ChunkCoord& center) const noexcept {
        const io::i32 rx = static_cast<io::i32>(render_distance_chunks);
        const io::i32 rz = static_cast<io::i32>(render_distance_chunks);
        const io::i32 ry = WorldYRadiusChunks();
        const io::u32 sx = static_cast<io::u32>(rx * 2 + 1);
        const io::u32 sz = static_cast<io::u32>(rz * 2 + 1);
        const io::usize plane = static_cast<io::usize>(sx) * static_cast<io::usize>(sz);

        const io::i32 oy = static_cast<io::i32>(index / plane) - ry;
        const io::usize rem = index % plane;
        const io::i32 oz = static_cast<io::i32>(rem / sx) - rz;
        const io::i32 ox = static_cast<io::i32>(rem % sx) - rx;

        return ge::voxel::ChunkCoord{
            center.x + ox,
            center.y + oy,
            center.z + oz
        };
    }

    inline ge::voxel::ChunkCoord ChunkCoordAtRequestGridIndex(io::usize index,
                                                               const ge::voxel::ChunkCoord& center) const noexcept {
        const io::i32 rx = static_cast<io::i32>(render_distance_chunks);
        const io::i32 rz = static_cast<io::i32>(render_distance_chunks);
        const io::i32 ry = RequestYRadiusChunks();
        const io::u32 sx = static_cast<io::u32>(rx * 2 + 1);
        const io::u32 sz = static_cast<io::u32>(rz * 2 + 1);
        const io::usize plane = static_cast<io::usize>(sx) * static_cast<io::usize>(sz);

        const io::i32 oy = static_cast<io::i32>(index / plane) - ry;
        const io::usize rem = index % plane;
        const io::i32 oz = static_cast<io::i32>(rem / sx) - rz;
        const io::i32 ox = static_cast<io::i32>(rem % sx) - rx;

        return ge::voxel::ChunkCoord{
            center.x + ox,
            center.y + oy,
            center.z + oz
        };
    }

