#include "../../../engine/core//config.hpp"

    static inline lm::mat4 Float16ToMat4(const ge::Modeller::Float16& src) noexcept {
        lm::mat4 out{};
        for (io::u32 c = 0; c < 4u; ++c)
            for (io::u32 r = 0; r < 4u; ++r)
                out[c][r] = src.v[c * 4u + r];
        return out;
    }

    static inline ge::Modeller::Float4 QuatNormalize(const ge::Modeller::Float4& q) noexcept {
        const float len2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (len2 <= 0.000001f)
            return ge::Modeller::Float4{ 0.f, 0.f, 0.f, 1.f };
        const float inv = 1.f / lm::sqrtf(len2);
        return ge::Modeller::Float4{ q.x * inv, q.y * inv, q.z * inv, q.w * inv };
    }

    static inline ge::Modeller::Float4 QuatNlerp(const ge::Modeller::Float4& a,
                                                  const ge::Modeller::Float4& b,
                                                  float t) noexcept {
        float bx = b.x, by = b.y, bz = b.z, bw = b.w;
        const float dot = a.x * bx + a.y * by + a.z * bz + a.w * bw;
        if (dot < 0.f) {
            bx = -bx; by = -by; bz = -bz; bw = -bw;
        }
        const float k0 = 1.f - t;
        const float k1 = t;
        const ge::Modeller::Float4 q{
            a.x * k0 + bx * k1,
            a.y * k0 + by * k1,
            a.z * k0 + bz * k1,
            a.w * k0 + bw * k1
        };
        return QuatNormalize(q);
    }

    static inline ge::Modeller::Float4 QuatMul(const ge::Modeller::Float4& a,
                                               const ge::Modeller::Float4& b) noexcept {
        return ge::Modeller::Float4{
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
        };
    }

    static inline lm::mat4 Mat4FromQuat(const ge::Modeller::Float4& qn) noexcept {
        const ge::Modeller::Float4 q = QuatNormalize(qn);
        const float a = q.w;
        const float b = q.x;
        const float c = q.y;
        const float d = q.z;
        const float a2 = a * a;
        const float b2 = b * b;
        const float c2 = c * c;
        const float d2 = d * d;
        lm::mat4 m{};
        m[0][0] = a2 + b2 - c2 - d2;
        m[0][1] = 2.f * (b * c + a * d);
        m[0][2] = 2.f * (b * d - a * c);
        m[0][3] = 0.f;
        m[1][0] = 2.f * (b * c - a * d);
        m[1][1] = a2 - b2 + c2 - d2;
        m[1][2] = 2.f * (c * d + a * b);
        m[1][3] = 0.f;
        m[2][0] = 2.f * (b * d + a * c);
        m[2][1] = 2.f * (c * d - a * b);
        m[2][2] = a2 - b2 - c2 + d2;
        m[2][3] = 0.f;
        m[3][0] = 0.f;
        m[3][1] = 0.f;
        m[3][2] = 0.f;
        m[3][3] = 1.f;
        return m;
    }

    static inline lm::mat4 ComposeTrs(const ge::Modeller::Float3& t,
                                      const ge::Modeller::Float4& r,
                                      const ge::Modeller::Float3& s) noexcept {
        const lm::mat4 tm = lm::mat4_translate(t.x, t.y, t.z);
        const lm::mat4 rm = Mat4FromQuat(r);
        const lm::mat4 sm = lm::mat4_scale(s.x, s.y, s.z);
        return tm * (rm * sm);
    }

    static inline float WrapTime(float t, float duration) noexcept {
        if (duration <= 0.000001f) return 0.f;
        while (t >= duration) t -= duration;
        while (t < 0.f) t += duration;
        return t;
    }

    static inline float NormalizeYawLocal(float angle) noexcept {
        while (angle > 180.f) angle -= 360.f;
        while (angle < -180.f) angle += 360.f;
        return angle;
    }

    static inline bool SampleChannelValue(const ge::Modeller::AnimationChannel& ch,
                                          float time_sec,
                                          ge::Modeller::Float4& out_v) noexcept {
        if (ch.times.empty() || ch.values.empty()) return false;
        const io::usize n = (ch.times.size() < ch.values.size()) ? ch.times.size() : ch.values.size();
        if (n == 0) return false;
        if (n == 1 || time_sec <= ch.times[0]) {
            out_v = ch.values[0];
            return true;
        }
        if (time_sec >= ch.times[n - 1u]) {
            out_v = ch.values[n - 1u];
            return true;
        }
        for (io::usize i = 1; i < n; ++i) {
            const float t1 = ch.times[i];
            if (time_sec > t1) continue;
            const float t0 = ch.times[i - 1u];
            const ge::Modeller::Float4& v0 = ch.values[i - 1u];
            const ge::Modeller::Float4& v1 = ch.values[i];
            float a = 0.f;
            const float dt = t1 - t0;
            if (dt > 0.000001f) a = (time_sec - t0) / dt;
            a = clampf(a, 0.f, 1.f);
            if (ch.path == ge::Modeller::AnimationPath::Rotation)
                out_v = QuatNlerp(v0, v1, a);
            else
                out_v = ge::Modeller::Float4{
                    v0.x + (v1.x - v0.x) * a,
                    v0.y + (v1.y - v0.y) * a,
                    v0.z + (v1.z - v0.z) * a,
                    v0.w + (v1.w - v0.w) * a
                };
            return true;
        }
        out_v = ch.values[n - 1u];
        return true;
    }

    IO_NODISCARD inline io::u32 ResolveEntityClipForAnim(io::u8 anim) const noexcept {
        if (anim == ge::net::WORLD_ACTOR_ANIM_LEVITATE) {
            if (entity_clip_levitate != ENTITY_CLIP_INVALID)
                return entity_clip_levitate;
            return entity_clip_stay;
        }
        return entity_clip_stay;
    }

    IO_NODISCARD inline io::u32 ResolvePlayerClipForState(io::u8 state) const noexcept {
        if (state == ge::net::PLAYER_ANIM_CRAWL_DOWN && player_clip_crawl_down != ENTITY_CLIP_INVALID)
            return player_clip_crawl_down;
        if (state == ge::net::PLAYER_ANIM_CRAWL_UP && player_clip_crawl_up != ENTITY_CLIP_INVALID)
            return player_clip_crawl_up;
        if (state == ge::net::PLAYER_ANIM_CRAWL_MOVE && player_clip_crawl != ENTITY_CLIP_INVALID)
            return player_clip_crawl;
        if (state == ge::net::PLAYER_ANIM_CRAWL_IDLE && player_clip_crawl != ENTITY_CLIP_INVALID)
            return player_clip_crawl;
        if (state == ge::net::PLAYER_ANIM_EAT && player_clip_eat != ENTITY_CLIP_INVALID)
            return player_clip_eat;
        if (state == ge::net::PLAYER_ANIM_RUN && player_clip_run != ENTITY_CLIP_INVALID)
            return player_clip_run;
        if (state == ge::net::PLAYER_ANIM_WALK && player_clip_walk != ENTITY_CLIP_INVALID)
            return player_clip_walk;
        if (player_clip_crawl != ENTITY_CLIP_INVALID &&
            (state == ge::net::PLAYER_ANIM_CRAWL_IDLE || state == ge::net::PLAYER_ANIM_CRAWL_MOVE ||
             state == ge::net::PLAYER_ANIM_CRAWL_DOWN || state == ge::net::PLAYER_ANIM_CRAWL_UP))
            return player_clip_crawl;
        if (player_clip_still != ENTITY_CLIP_INVALID)
            return player_clip_still;
        if (player_clip_walk != ENTITY_CLIP_INVALID)
            return player_clip_walk;
        if (player_clip_run != ENTITY_CLIP_INVALID)
            return player_clip_run;
        if (player_clip_eat != ENTITY_CLIP_INVALID)
            return player_clip_eat;
        return ENTITY_CLIP_INVALID;
    }

    inline void ApplyExtraBoneLook(io::u32 bone_count, io::i32 bone_index,
                                   float yaw_deg, float pitch_deg) noexcept {
        if (bone_index < 0 || static_cast<io::u32>(bone_index) >= bone_count) return;
        const float hy = yaw_deg * 0.5f * 0.0174532925f;
        const float hp = pitch_deg * 0.5f * 0.0174532925f;
        const ge::Modeller::Float4 yaw_q{ 0.f, lm::sinf(hy), 0.f, lm::cosf(hy) };
        const ge::Modeller::Float4 pitch_q{ lm::sinf(hp), 0.f, 0.f, lm::cosf(hp) };
        const ge::Modeller::Float4 add_q = QuatMul(yaw_q, pitch_q);
        ge::Modeller::Float4& base_q = entity_pose_r[static_cast<io::u32>(bone_index)];
        // Apply additive look in local-bone space over current animation.
        base_q = QuatNormalize(QuatMul(base_q, add_q));
    }

    inline void ApplySneakOverlay(io::u32 bone_count) noexcept {
        if (player_torso_bone_index >= 0 && static_cast<io::u32>(player_torso_bone_index) < bone_count) {
            ge::Modeller::Float3& torso_t = entity_pose_t[static_cast<io::u32>(player_torso_bone_index)];
            torso_t.y -= 0.17f;
            ApplyExtraBoneLook(bone_count, player_torso_bone_index, 0.f, -10.f);
        }

    const float leg_pitch = 22.f;
        ApplyExtraBoneLook(bone_count, player_left_leg_bone_index, 0.f, leg_pitch);
        ApplyExtraBoneLook(bone_count, player_right_leg_bone_index, 0.f, leg_pitch);
    }

    inline void UpdateEntityAnimatorForClip(io::usize model_index, io::u32 clip_index, float clip_time,
                                            io::i32 extra_rot_bone0 = -1, float extra_yaw_deg0 = 0.f, float extra_pitch_deg0 = 0.f,
                                            io::i32 extra_rot_bone1 = -1, float extra_yaw_deg1 = 0.f, float extra_pitch_deg1 = 0.f,
                                            bool apply_sneak = false) noexcept {
        if (model_index == static_cast<io::usize>(-1)) return;
        if (model_index >= model_data.models.size()) return;
        const ge::Modeller::ImportedModel& model = model_data.models[model_index];
        if (model.kind != ge::Modeller::ModelKind::Entity) return;
        if (model.bones.empty()) return;

        io::u32 bone_count = static_cast<io::u32>(model.bones.size());
        if (bone_count > ENTITY_BONE_CAP) bone_count = ENTITY_BONE_CAP;
        entity_bone_count = bone_count;
        for (io::u32 i = 0; i < bone_count; ++i) {
            entity_pose_t[i] = model.bones[i].bind_t;
            entity_pose_r[i] = model.bones[i].bind_r;
            entity_pose_s[i] = model.bones[i].bind_s;
        }

        if (!model.clips.empty() && clip_index != ENTITY_CLIP_INVALID && clip_index < model.clips.size()) {
            const ge::Modeller::AnimationClip& clip = model.clips[clip_index];
            const float clip_t = WrapTime(clip_time, clip.duration_sec);
            for (io::usize ci = 0; ci < clip.channels.size(); ++ci) {
                const ge::Modeller::AnimationChannel& ch = clip.channels[ci];
                if (ch.bone_index >= bone_count) continue;
                ge::Modeller::Float4 sv{};
                if (!SampleChannelValue(ch, clip_t, sv)) continue;
                if (ch.path == ge::Modeller::AnimationPath::Translation)
                    entity_pose_t[ch.bone_index] = ge::Modeller::Float3{ sv.x, sv.y, sv.z };
                else if (ch.path == ge::Modeller::AnimationPath::Rotation)
                    entity_pose_r[ch.bone_index] = sv;
                else
                    entity_pose_s[ch.bone_index] = ge::Modeller::Float3{ sv.x, sv.y, sv.z };
            }
        }

        ApplyExtraBoneLook(bone_count, extra_rot_bone0, extra_yaw_deg0, extra_pitch_deg0);
        ApplyExtraBoneLook(bone_count, extra_rot_bone1, extra_yaw_deg1, extra_pitch_deg1);
        if (apply_sneak)
            ApplySneakOverlay(bone_count);

        for (io::u32 i = 0; i < bone_count; ++i)
            entity_local_mats[i] = ComposeTrs(entity_pose_t[i], entity_pose_r[i], entity_pose_s[i]);
        for (io::u32 i = 0; i < bone_count; ++i) {
            const io::i32 parent = model.bones[i].parent_bone;
            if (parent >= 0 && static_cast<io::u32>(parent) < bone_count)
                entity_global_mats[i] = entity_global_mats[static_cast<io::u32>(parent)] * entity_local_mats[i];
            else
                entity_global_mats[i] = entity_local_mats[i];
        }
        for (io::u32 i = 0; i < bone_count; ++i)
            entity_bones[i] = entity_global_mats[i] * Float16ToMat4(model.bones[i].inverse_bind);
    }

    inline void UpdateEntityAnimator() noexcept {
        UpdateEntityAnimatorForClip(entity_model_index, entity_anim_clip_index, frame.scene_time);
    }

    inline void DrawEntityMesh(const lm::mat4& model, gl::VertexArray& vao_ref, gl::Buffer& ebo_ref, io::u32 index_count) noexcept {
        if (index_count == 0u) return;
        if (entity_uniforms.u_model >= 0)
            gl::UniformMatrix4fv(entity_uniforms.u_model, 1, false, model[0].data());
        vao_ref.bind();
        ebo_ref.bind();
        gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(index_count),
                         gl::DrawElementsType::UnsignedInt, nullptr);
    }

    inline void DrawEntity(const lm::mat4& model) noexcept {
        DrawEntityMesh(model, entity_vao, entity_ebo, entity_index_count);
    }

    inline void DrawWorldItemActors(const lm::vec3& camera_world) noexcept {
        if (!world_actor_ecs || item_sprite_index_count == 0u) return;
        terrain_shader.Use();
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        if (terrain_uniforms.u_atlas >= 0)
            gl::Uniform1i(terrain_uniforms.u_atlas, 0);
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 0);
        if (terrain_uniforms.u_model >= 0) {
            const lm::mat4 identity = lm::mat4_identity();
            gl::UniformMatrix4fv(terrain_uniforms.u_model, 1, false, identity[0].data());
        }

        const lm::vec3 normal = lm::vec3{ -camera.front[0], -camera.front[1], -camera.front[2] };
        item_sprite_vao.bind();
        item_sprite_ebo.bind();
        for (io::u32 i = 0; i < WORLD_ACTOR_CAP; ++i) {
            ActorEcs& ecs = *world_actor_ecs;
            if (ecs.alive[i] == 0u) continue;
            if (!ecs.net_sync[i].active) continue;
            const io::u8 model = ecs.identity[i].model;
            if (model != ge::net::WORLD_ACTOR_MODEL_ITEM &&
                model != ge::net::WORLD_ACTOR_MODEL_SPELL) continue;
            if (!ecs.render_model[i].visible) continue;

            ge::item::Stack draw_stack = ecs.item_drop[i].stack;
            if (model == ge::net::WORLD_ACTOR_MODEL_SPELL) {
                ge::item::Id sid = static_cast<ge::item::Id>(ecs.mob_state[i].net_state);
                if (!ge::item::valid(sid)) continue;
                draw_stack = ge::item::make_stack(sid, 1u);
            } else {
                ge::item::normalize(draw_stack);
                if (ge::item::is_empty(draw_stack)) continue;
                if (ge::item::uses_book_model(draw_stack.id)) continue;
            }

            float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
            if (!ItemIconUv(draw_stack, u0, v0, u1, v1))
                continue;

            const float rel_x = ecs.transform[i].x - camera_world[0];
            const float rel_y = ecs.transform[i].y - camera_world[1];
            const float rel_z = ecs.transform[i].z - camera_world[2];
            const bool grounded = (model == ge::net::WORLD_ACTOR_MODEL_ITEM) ? ecs.item_drop[i].grounded : false;
            const float size = (model == ge::net::WORLD_ACTOR_MODEL_SPELL)
                ? 0.26f
                : (grounded ? 0.34f : 0.30f);
            const float center_y = (model == ge::net::WORLD_ACTOR_MODEL_SPELL)
                ? (rel_y + 0.02f)
                : (rel_y + (grounded ? 0.18f : 0.10f));
            const lm::vec3 center{ rel_x, center_y, rel_z };
            const lm::vec3 right = camera.right * size;
            const lm::vec3 up = camera.up * size;

            ge::voxel::MeshVertex verts[4]{};
            const lm::vec3 positions[4]{
                center - right - up,
                center + right - up,
                center + right + up,
                center - right + up
            };
            const float uvs[4][2]{
                { u0, v1 },
                { u1, v1 },
                { u1, v0 },
                { u0, v0 }
            };

            for (io::u32 v = 0u; v < 4u; ++v) {
                verts[v].px = positions[v][0];
                verts[v].py = positions[v][1];
                verts[v].pz = positions[v][2];
                verts[v].nx = normal[0];
                verts[v].ny = normal[1];
                verts[v].nz = normal[2];
                verts[v].u = uvs[v][0];
                verts[v].v = uvs[v][1];
                verts[v].atlas_u0 = u0;
                verts[v].atlas_v0 = v0;
                verts[v].atlas_u1 = u1;
                verts[v].atlas_v1 = v1;
                verts[v].block_id = ge::voxel::block_index(ge::voxel::BlockId::Grass);
                verts[v].face = 4u;
                verts[v].ao = 255u;
            }

            item_sprite_vbo.bind();
            item_sprite_vbo.data(verts, sizeof(verts), gl::BufferUsage::DynamicDraw);
            gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(item_sprite_index_count),
                             gl::DrawElementsType::UnsignedInt, nullptr);
        }
    }

    IO_NODISCARD static inline lm::vec3 TransformPointMat4(const lm::mat4& m, float x, float y, float z) noexcept {
        return lm::vec3{
            m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0],
            m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1],
            m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2]
        };
    }

    IO_NODISCARD static inline lm::vec3 Vec3Sub(const lm::vec3& a, const lm::vec3& b) noexcept {
        return lm::vec3{ a[0] - b[0], a[1] - b[1], a[2] - b[2] };
    }

    IO_NODISCARD static inline lm::vec3 Vec3Cross(const lm::vec3& a, const lm::vec3& b) noexcept {
        return lm::vec3{
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]
        };
    }

    IO_NODISCARD static inline bool HeldItemPseudo3D(const ge::item::Stack& stack) noexcept {
        if (ge::item::is_empty(stack)) return false;
        if (stack.id == ge::item::Id::RustyDagger) return true;
        return ge::item::def(stack.id).category == ge::item::Category::SpellingWards;
    }

    inline void DrawHeldItemQuadUv4(const lm::vec3& p0, const lm::vec3& p1, const lm::vec3& p2, const lm::vec3& p3,
                                    float uv0_u, float uv0_v,
                                    float uv1_u, float uv1_v,
                                    float uv2_u, float uv2_v,
                                    float uv3_u, float uv3_v,
                                    float atlas_u0, float atlas_v0, float atlas_u1, float atlas_v1) noexcept {
        const lm::vec3 e0 = Vec3Sub(p1, p0);
        const lm::vec3 e1 = Vec3Sub(p2, p0);
        const lm::vec3 n = lm::vec3_norm(Vec3Cross(e0, e1));

        ge::voxel::MeshVertex verts[4]{};
        const lm::vec3 positions[4]{ p0, p1, p2, p3 };
        const float uvs[4][2]{
            { uv0_u, uv0_v },
            { uv1_u, uv1_v },
            { uv2_u, uv2_v },
            { uv3_u, uv3_v }
        };
        for (io::u32 v = 0u; v < 4u; ++v) {
            verts[v].px = positions[v][0];
            verts[v].py = positions[v][1];
            verts[v].pz = positions[v][2];
            verts[v].nx = n[0];
            verts[v].ny = n[1];
            verts[v].nz = n[2];
            verts[v].u = uvs[v][0];
            verts[v].v = uvs[v][1];
            verts[v].atlas_u0 = atlas_u0;
            verts[v].atlas_v0 = atlas_v0;
            verts[v].atlas_u1 = atlas_u1;
            verts[v].atlas_v1 = atlas_v1;
            verts[v].block_id = ge::voxel::block_index(ge::voxel::BlockId::Grass);
            verts[v].face = 4u;
            verts[v].ao = 255u;
        }
        item_sprite_vbo.bind();
        item_sprite_vbo.data(verts, sizeof(verts), gl::BufferUsage::DynamicDraw);
        gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(item_sprite_index_count),
                         gl::DrawElementsType::UnsignedInt, nullptr);
    }

    inline void DrawHeldItemQuad(const lm::vec3& p0, const lm::vec3& p1, const lm::vec3& p2, const lm::vec3& p3,
                                 float u0, float v0, float u1, float v1,
                                 bool flip_u = false,
                                 bool flip_v = false) noexcept {
        const float uu0 = flip_u ? u1 : u0;
        const float uu1 = flip_u ? u0 : u1;
        const float vv0 = flip_v ? v1 : v0;
        const float vv1 = flip_v ? v0 : v1;
        DrawHeldItemQuadUv4(p0, p1, p2, p3,
                            uu0, vv0,
                            uu1, vv0,
                            uu1, vv1,
                            uu0, vv1,
                            u0, v0, u1, v1);
    }

    IO_NODISCARD static inline bool MatrixMirrored(const lm::mat4& m) noexcept {
        const lm::vec3 x{ m[0][0], m[0][1], m[0][2] };
        const lm::vec3 y{ m[1][0], m[1][1], m[1][2] };
        const lm::vec3 z{ m[2][0], m[2][1], m[2][2] };
        const lm::vec3 c = Vec3Cross(x, y);
        const float det_like = c[0] * z[0] + c[1] * z[1] + c[2] * z[2];
        return det_like < 0.f;
    }

    inline void PrepareHeldItemUv(const ge::item::Stack& stack,
                                  float& u0, float& v0, float& u1, float& v1,
                                  bool inset = true) noexcept {
        if (!ItemIconUv(stack, u0, v0, u1, v1))
            return;
        if (inset && texture_atlas.atlas_width > 0u && texture_atlas.atlas_height > 0u) {
            const float du = 0.5f / static_cast<float>(texture_atlas.atlas_width);
            const float dv = 0.5f / static_cast<float>(texture_atlas.atlas_height);
            const float min_u = (u0 < u1) ? u0 : u1;
            const float max_u = (u0 < u1) ? u1 : u0;
            const float min_v = (v0 < v1) ? v0 : v1;
            const float max_v = (v0 < v1) ? v1 : v0;
            u0 = min_u + du;
            u1 = max_u - du;
            v0 = min_v + dv;
            v1 = max_v - dv;
        }
    }

    inline void DrawHeldItemPseudo3DGeometry(const lm::mat4& anchor,
                                             float u0, float v0, float u1, float v1,
                                             float size,
                                             bool flip_u,
                                             bool flip_v,
                                             const Pseudo3dSeamUv* seam_uv = nullptr) noexcept {
        (void)seam_uv;
        const float t = size * 0.08f;
        const lm::vec3 f0 = TransformPointMat4(anchor, -size, -size,  t);
        const lm::vec3 f1 = TransformPointMat4(anchor,  size, -size,  t);
        const lm::vec3 f2 = TransformPointMat4(anchor,  size,  size,  t);
        const lm::vec3 f3 = TransformPointMat4(anchor, -size,  size,  t);
        const lm::vec3 b0 = TransformPointMat4(anchor, -size, -size, -t);
        const lm::vec3 b1 = TransformPointMat4(anchor,  size, -size, -t);
        const lm::vec3 b2 = TransformPointMat4(anchor,  size,  size, -t);
        const lm::vec3 b3 = TransformPointMat4(anchor, -size,  size, -t);
        DrawHeldItemQuad(f0, f1, f2, f3, u0, v0, u1, v1, flip_u, flip_v);
        DrawHeldItemQuad(b1, b0, b3, b2, u0, v0, u1, v1, !flip_u, flip_v);
        if (!texture_atlas.atlas_pixels || texture_atlas.atlas_width == 0u || texture_atlas.atlas_height == 0u)
            return;
        const io::u32 aw = texture_atlas.atlas_width;
        const io::u32 ah = texture_atlas.atlas_height;
        const io::u32 ch = (texture_atlas.atlas_channels > 0u) ? texture_atlas.atlas_channels : 4u;
        const float min_u = (u0 < u1) ? u0 : u1;
        const float max_u = (u0 < u1) ? u1 : u0;
        const float min_v = (v0 < v1) ? v0 : v1;
        const float max_v = (v0 < v1) ? v1 : v0;

        io::i32 x0i = static_cast<io::i32>(lm::floorf(min_u * static_cast<float>(aw)));
        io::i32 y0i = static_cast<io::i32>(lm::floorf(min_v * static_cast<float>(ah)));
        io::i32 x1i = static_cast<io::i32>(-lm::floorf(-(max_u * static_cast<float>(aw)))) - 1;
        io::i32 y1i = static_cast<io::i32>(-lm::floorf(-(max_v * static_cast<float>(ah)))) - 1;
        if (x1i < x0i || y1i < y0i) return;

        const io::u32 x0 = ClampTexelCoord(x0i, aw);
        const io::u32 y0 = ClampTexelCoord(y0i, ah);
        const io::u32 x1 = ClampTexelCoord(x1i, aw);
        const io::u32 y1 = ClampTexelCoord(y1i, ah);
        if (x1 < x0 || y1 < y0) return;
        const io::u32 pw = x1 - x0 + 1u;
        const io::u32 ph = y1 - y0 + 1u;
        if (pw == 0u || ph == 0u) return;

        const io::u8 alpha_threshold = 14u;
        const float inv_wf = 1.0f / static_cast<float>(pw);
        const float inv_hf = 1.0f / static_cast<float>(ph);

        const auto opaque_at = [&](io::i32 ix, io::i32 iy) noexcept {
            if (ix < 0 || iy < 0) return false;
            const io::u32 ux = static_cast<io::u32>(ix);
            const io::u32 uy = static_cast<io::u32>(iy);
            if (ux >= pw || uy >= ph) return false;
            const io::u32 ax = x0 + ux;
            const io::u32 ay = y0 + uy;
            const io::usize p = (static_cast<io::usize>(ay) * aw + ax) * ch;
            const io::u8 a = (ch >= 4u) ? texture_atlas.atlas_pixels[p + 3u] : 255u;
            return a >= alpha_threshold;
        };
        const auto pixel_center_u = [&](io::u32 ix) noexcept {
            return (static_cast<float>(x0 + ix) + 0.5f) / static_cast<float>(aw);
        };
        const auto pixel_center_v = [&](io::u32 iy) noexcept {
            return (static_cast<float>(y0 + iy) + 0.5f) / static_cast<float>(ah);
        };
        const auto local_x_l = [&](io::u32 ix) noexcept {
            return -size + (static_cast<float>(ix) * inv_wf) * (2.0f * size);
        };
        const auto local_x_r = [&](io::u32 ix) noexcept {
            return -size + (static_cast<float>(ix + 1u) * inv_wf) * (2.0f * size);
        };
        const auto local_y_t = [&](io::u32 iy) noexcept {
            return size - (static_cast<float>(iy) * inv_hf) * (2.0f * size);
        };
        const auto local_y_b = [&](io::u32 iy) noexcept {
            return size - (static_cast<float>(iy + 1u) * inv_hf) * (2.0f * size);
        };

        for (io::u32 iy = 0u; iy < ph; ++iy) {
            for (io::u32 ix = 0u; ix < pw; ++ix) {
                if (!opaque_at(static_cast<io::i32>(ix), static_cast<io::i32>(iy)))
                    continue;
                const float cu = pixel_center_u(ix);
                const float cv0 = pixel_center_v(iy);
                const float cv = flip_v ? (min_v + max_v - cv0) : cv0;
                const float xl = local_x_l(ix);
                const float xr = local_x_r(ix);
                const float yt = local_y_t(iy);
                const float yb = local_y_b(iy);

                if (!opaque_at(static_cast<io::i32>(ix) - 1, static_cast<io::i32>(iy))) {
                    const lm::vec3 p0 = TransformPointMat4(anchor, xl, yb,  t);
                    const lm::vec3 p1 = TransformPointMat4(anchor, xl, yt,  t);
                    const lm::vec3 p2 = TransformPointMat4(anchor, xl, yt, -t);
                    const lm::vec3 p3 = TransformPointMat4(anchor, xl, yb, -t);
                    DrawHeldItemQuadUv4(p0, p1, p2, p3, cu, cv, cu, cv, cu, cv, cu, cv, u0, v0, u1, v1);
                }
                if (!opaque_at(static_cast<io::i32>(ix) + 1, static_cast<io::i32>(iy))) {
                    const lm::vec3 p0 = TransformPointMat4(anchor, xr, yb,  t);
                    const lm::vec3 p1 = TransformPointMat4(anchor, xr, yb, -t);
                    const lm::vec3 p2 = TransformPointMat4(anchor, xr, yt, -t);
                    const lm::vec3 p3 = TransformPointMat4(anchor, xr, yt,  t);
                    DrawHeldItemQuadUv4(p0, p1, p2, p3, cu, cv, cu, cv, cu, cv, cu, cv, u0, v0, u1, v1);
                }
                if (!opaque_at(static_cast<io::i32>(ix), static_cast<io::i32>(iy) - 1)) {
                    const lm::vec3 p0 = TransformPointMat4(anchor, xl, yt,  t);
                    const lm::vec3 p1 = TransformPointMat4(anchor, xr, yt,  t);
                    const lm::vec3 p2 = TransformPointMat4(anchor, xr, yt, -t);
                    const lm::vec3 p3 = TransformPointMat4(anchor, xl, yt, -t);
                    DrawHeldItemQuadUv4(p0, p1, p2, p3, cu, cv, cu, cv, cu, cv, cu, cv, u0, v0, u1, v1);
                }
                if (!opaque_at(static_cast<io::i32>(ix), static_cast<io::i32>(iy) + 1)) {
                    const lm::vec3 p0 = TransformPointMat4(anchor, xl, yb,  t);
                    const lm::vec3 p1 = TransformPointMat4(anchor, xl, yb, -t);
                    const lm::vec3 p2 = TransformPointMat4(anchor, xr, yb, -t);
                    const lm::vec3 p3 = TransformPointMat4(anchor, xr, yb,  t);
                    DrawHeldItemQuadUv4(p0, p1, p2, p3, cu, cv, cu, cv, cu, cv, cu, cv, u0, v0, u1, v1);
                }
            }
        }
    }

    inline void DrawRemoteHeldItem(const RemotePlayerVisual& rp, const lm::mat4& player_model) noexcept {
        if (item_sprite_index_count == 0u) return;
        if (!ge::item::valid(rp.held_item) || rp.held_item == ge::item::Id::None) return;
        ge::item::Stack stack = ge::item::make_stack(rp.held_item, 1u);
        ge::item::normalize(stack);
        if (ge::item::is_empty(stack)) return;

        const bool pseudo3d = HeldItemPseudo3D(stack);
        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        PrepareHeldItemUv(stack, u0, v0, u1, v1, !pseudo3d);
        if (u0 == u1 || v0 == v1) return;

        lm::mat4 anchor = player_model;
        if (player_right_arm_bone_index >= 0 && static_cast<io::u32>(player_right_arm_bone_index) < player_entity_bone_count) {
            anchor = anchor * entity_global_mats[static_cast<io::u32>(player_right_arm_bone_index)];
            // Hand-local attachment (tuned for this player model rig).
            anchor = anchor * lm::mat4_translate(0.02f, -0.82f, 0.02f);
            anchor = anchor * lm::mat4_rotate_z(lm::radians(20.f));
            anchor = anchor * lm::mat4_rotate_x(lm::radians(-14.f));
            anchor = anchor * lm::mat4_rotate_y(lm::radians(-18.f));
        } else if (player_torso_bone_index >= 0 && static_cast<io::u32>(player_torso_bone_index) < player_entity_bone_count) {
            // Safe fallback for models where right-arm bone name is non-standard.
            anchor = anchor * entity_global_mats[static_cast<io::u32>(player_torso_bone_index)];
            anchor = anchor * lm::mat4_translate(0.34f, 0.78f, 0.04f);
            anchor = anchor * lm::mat4_rotate_z(lm::radians(18.f));
            anchor = anchor * lm::mat4_rotate_x(lm::radians(-10.f));
        } else {
            anchor = anchor * lm::mat4_translate(0.34f, 0.88f, 0.08f);
        }

        const float s2d = 0.17f;
        const float s = pseudo3d ? (s2d * 2.0f) : s2d;
        const bool flip_v = true;
        if (pseudo3d) {
            // Minecraft-like grip pivot: the lower-left sprite corner starts from the hand.
            anchor = anchor * lm::mat4_translate(s, s, 0.f);
            anchor = anchor * lm::mat4_rotate_z(lm::radians(65.f));
            anchor = anchor * lm::mat4_rotate_x(lm::radians(-25.f));
            anchor = anchor * lm::mat4_rotate_y(lm::radians(180.f - 25.f));
            // Final settle: keep it in front, but lift and tighten into palm grip a bit more.
            anchor = anchor * lm::mat4_translate(s * 0.35f, s * 1.35f, s * 0.05f);
        }
        const bool flip_u = MatrixMirrored(anchor);
        Pseudo3dSeamUv seam{};
        const Pseudo3dSeamUv* seam_ptr = nullptr;
        if (pseudo3d && Pseudo3dSeamUvForItem(stack, seam))
            seam_ptr = &seam;

        terrain_shader.Use();
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        if (terrain_uniforms.u_atlas >= 0)
            gl::Uniform1i(terrain_uniforms.u_atlas, 0);
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 0);
        if (terrain_uniforms.u_model >= 0) {
            const lm::mat4 identity = lm::mat4_identity();
            gl::UniformMatrix4fv(terrain_uniforms.u_model, 1, false, identity[0].data());
        }
        item_sprite_vao.bind();
        item_sprite_ebo.bind();

        if (pseudo3d) {
            DrawHeldItemPseudo3DGeometry(anchor, u0, v0, u1, v1, s, flip_u, flip_v, seam_ptr);
        } else {
            const lm::vec3 q0 = TransformPointMat4(anchor, -s, -s, 0.f);
            const lm::vec3 q1 = TransformPointMat4(anchor,  s, -s, 0.f);
            const lm::vec3 q2 = TransformPointMat4(anchor,  s,  s, 0.f);
            const lm::vec3 q3 = TransformPointMat4(anchor, -s,  s, 0.f);
            DrawHeldItemQuad(q0, q1, q2, q3, u0, v0, u1, v1, flip_u, flip_v);
        }
        entity_shader.Use();
    }

    inline void DrawFirstPersonHeldItem(const lm::vec3& camera_world) noexcept {
        if (screen != ScreenState::InGame) return;
        if (inventory_open || player_dead) return;
        const ge::item::Stack selected = SelectedHotbarStack();
        if (ge::item::is_empty(selected)) return;

        const bool pseudo3d = HeldItemPseudo3D(selected);
        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        PrepareHeldItemUv(selected, u0, v0, u1, v1, !pseudo3d);
        if (u0 == u1 || v0 == v1) return;
        const float s2d = 0.19f;
        const float s = pseudo3d ? (s2d * 2.0f) : s2d;
        Pseudo3dSeamUv seam{};
        const Pseudo3dSeamUv* seam_ptr = nullptr;
        if (pseudo3d && Pseudo3dSeamUvForItem(selected, seam))
            seam_ptr = &seam;

        // Viewmodel anchor in camera-space (relative world), with Minecraft-like right-hand bias.
        lm::mat4 anchor = lm::mat4_translate(camera.right[0] * 0.38f + camera.front[0] * 0.62f + camera.up[0] * -0.30f,
                                             camera.right[1] * 0.38f + camera.front[1] * 0.62f + camera.up[1] * -0.30f,
                                             camera.right[2] * 0.38f + camera.front[2] * 0.62f + camera.up[2] * -0.30f);
        anchor = anchor * lm::mat4_rotate_y(lm::radians(160.f));
        anchor = anchor * lm::mat4_rotate_x(lm::radians(-18.f));
        anchor = anchor * lm::mat4_rotate_z(lm::radians(-12.f));
        if (pseudo3d) {
            // Dagger/staves orientation in first-person: blade/top should face up, not upside-down.
            anchor = anchor * lm::mat4_rotate_z(lm::radians(180.f));
            // Make pseudo-3D orientation react to horizontal look direction.
            anchor = anchor * lm::mat4_rotate_y(lm::radians(NormalizeYawLocal(camera.yaw)));
        }

        (void)camera_world;
        terrain_shader.Use();
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        if (terrain_uniforms.u_atlas >= 0)
            gl::Uniform1i(terrain_uniforms.u_atlas, 0);
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 0);
        if (terrain_uniforms.u_model >= 0) {
            const lm::mat4 identity = lm::mat4_identity();
            gl::UniformMatrix4fv(terrain_uniforms.u_model, 1, false, identity[0].data());
        }
        item_sprite_vao.bind();
        item_sprite_ebo.bind();

        const bool flip_u = MatrixMirrored(anchor);
        const bool flip_v = false;
        if (pseudo3d) {
            DrawHeldItemPseudo3DGeometry(anchor, u0, v0, u1, v1, s, flip_u, flip_v, seam_ptr);
        } else {
            // First-person non-weapon/ward items are always rendered as billboards.
            const lm::vec3 center{ anchor[3][0], anchor[3][1], anchor[3][2] };
            const lm::vec3 right = lm::vec3_norm(camera.right) * s;
            const lm::vec3 up = lm::vec3_norm(camera.up) * s;
            const lm::vec3 q0 = center - right - up;
            const lm::vec3 q1 = center + right - up;
            const lm::vec3 q2 = center + right + up;
            const lm::vec3 q3 = center - right + up;
            DrawHeldItemQuad(q0, q1, q2, q3, u0, v0, u1, v1, false, false);
        }
    }

    inline void DrawWorldActors(const lm::vec3& camera_world, float dt) noexcept {
        if (!world_actor_ecs) return;
        if (entity_index_count == 0u) return;
        if (entity_model_index == static_cast<io::usize>(-1) || entity_model_index >= model_data.models.size()) return;
        ActorEcs& ecs = *world_actor_ecs;
        const ge::Modeller::ImportedModel& model_def = model_data.models[entity_model_index];
        if (model_def.kind != ge::Modeller::ModelKind::Entity) return;
        const float step_dt = clampf(dt, 0.f, 0.06f);

        for (io::u32 i = 0; i < WORLD_ACTOR_CAP; ++i) {
            if (ecs.alive[i] == 0u) continue;
            if (!ecs.net_sync[i].active) continue;
            if (!ecs.render_model[i].visible) continue;
            if (ecs.identity[i].model == ge::net::WORLD_ACTOR_MODEL_ITEM &&
                !ge::item::uses_book_model(ecs.item_drop[i].stack.id))
                continue;
            if (ecs.identity[i].model != ge::net::WORLD_ACTOR_MODEL_LEVITATING_BOOK &&
                ecs.identity[i].model != ge::net::WORLD_ACTOR_MODEL_ITEM)
                continue;

            const io::u32 clip_index = ResolveEntityClipForAnim(ecs.mob_state[i].net_anim);
            float clip_duration = 0.f;
            if (clip_index < model_def.clips.size())
                clip_duration = model_def.clips[clip_index].duration_sec;
            if (ecs.animator[i].current_clip != clip_index) {
                ecs.animator[i].next_clip = clip_index;
                ecs.animator[i].current_clip = clip_index;
                ecs.animator[i].clip_time = 0.f;
                ecs.animator[i].blend_alpha = 0.f;
            }

            ecs.animator[i].clip_time += step_dt * ecs.animator[i].playback_speed;
            if (clip_duration > 0.000001f)
                ecs.animator[i].clip_time = WrapTime(ecs.animator[i].clip_time, clip_duration);
            else
                ecs.animator[i].clip_time = 0.f;
            UpdateEntityAnimatorForClip(entity_model_index, clip_index, ecs.animator[i].clip_time);
            if (entity_uniforms.u_bones0 >= 0 && entity_bone_count > 0u)
                gl::UniformMatrix4fv(entity_uniforms.u_bones0, static_cast<int>(entity_bone_count), false, entity_bones[0][0].data());

            const float rel_x = ecs.transform[i].x - camera_world[0];
            const float rel_y = ecs.transform[i].y - camera_world[1];
            const float rel_z = ecs.transform[i].z - camera_world[2];
            lm::mat4 model = lm::mat4_translate(rel_x, rel_y, rel_z);
            if (ecs.identity[i].mode == ge::net::WORLD_ACTOR_MODE_MOB) {
                const float wobble = lm::sinf(frame.scene_time * 2.2f + static_cast<float>(ecs.identity[i].actor_id) * 0.25f) * 0.06f;
                model = model * lm::mat4_translate(0.f, wobble, 0.f);
            }
            DrawEntity(model);
        }
    }

    inline void DrawRemotePlayers(const lm::vec3& camera_world, float dt) noexcept {
        if (!remote_players) return;
        if (player_entity_index_count == 0u) return;
        if (player_entity_model_index == static_cast<io::usize>(-1) || player_entity_model_index >= model_data.models.size()) return;
        const ge::Modeller::ImportedModel& model_def = model_data.models[player_entity_model_index];
        if (model_def.kind != ge::Modeller::ModelKind::Entity) return;

        RemotePlayerVisual snapshot[REMOTE_PLAYER_CAP]{};
        io::u32 snapshot_count = 0u;
        remote_players_lock.lock();
        for (io::u32 i = 0u; i < REMOTE_PLAYER_CAP; ++i) {
            const RemotePlayerVisual& rp = remote_players[i];
            if (!rp.active) continue;
            if (snapshot_count >= REMOTE_PLAYER_CAP) break;
            snapshot[snapshot_count++] = rp;
        }
        remote_players_lock.unlock();

        const float step_dt = clampf(dt, 0.f, 0.06f);
        static constexpr float PLAYER_EYE_TO_FEET = 1.62f;
        static constexpr float PLAYER_CRAWL_EYE_TO_FEET = 1.00f;
        for (io::u32 i = 0u; i < snapshot_count; ++i) {
            const RemotePlayerVisual& rp = snapshot[i];
            io::u32 clip_index = ResolvePlayerClipForState(rp.state);
            if (clip_index == ENTITY_CLIP_INVALID && !model_def.clips.empty())
                clip_index = 0u;
            float clip_time = rp.anim_time;
            if (clip_index < model_def.clips.size()) {
                const float dur = model_def.clips[clip_index].duration_sec;
                if (dur > 0.000001f) clip_time = WrapTime(clip_time + step_dt, dur);
                else clip_time = 0.f;
            } else {
                clip_time = 0.f;
            }

            const float look_delta = NormalizeYawLocal(rp.body_yaw - rp.yaw);
            const float torso_yaw = clampf(look_delta * 0.35f, -22.f, 22.f);
            const float head_yaw = clampf(look_delta - torso_yaw, -85.f, 85.f);
            const bool crawling = ((rp.action_flags & ge::net::PLAYER_ACTION_FLAG_CRAWL) != 0u) ||
                                  rp.state == ge::net::PLAYER_ANIM_CRAWL_IDLE ||
                                  rp.state == ge::net::PLAYER_ANIM_CRAWL_MOVE ||
                                  rp.state == ge::net::PLAYER_ANIM_CRAWL_DOWN ||
                                  rp.state == ge::net::PLAYER_ANIM_CRAWL_UP;
            const bool sneaking = !crawling && (rp.action_flags & ge::net::PLAYER_ACTION_FLAG_SNEAK) != 0u;
            UpdateEntityAnimatorForClip(player_entity_model_index, clip_index, clip_time,
                                        player_torso_bone_index, torso_yaw, 0.f,
                                        player_head_bone_index, head_yaw, rp.pitch,
                                        sneaking);
            if (entity_uniforms.u_bones0 >= 0 && player_entity_bone_count > 0u)
                gl::UniformMatrix4fv(entity_uniforms.u_bones0, static_cast<int>(player_entity_bone_count), false, entity_bones[0][0].data());

            const float rel_x = rp.x - camera_world[0];
            const float rel_y = rp.y - camera_world[1];
            const float rel_z = rp.z - camera_world[2];
            const float eye_to_feet = crawling ? PLAYER_CRAWL_EYE_TO_FEET : PLAYER_EYE_TO_FEET;
            lm::mat4 model = lm::mat4_translate(rel_x, rel_y - eye_to_feet, rel_z);
            model = model * lm::mat4_rotate_y(lm::radians(-rp.body_yaw - 90.f));
            DrawEntityMesh(model, player_entity_vao, player_entity_ebo, player_entity_index_count);
            DrawRemoteHeldItem(rp, model);
        }
    }

    inline void DrawMainMenuPlayer(const lm::mat4& view,
                                   const lm::mat4& proj,
                                   const lm::vec3& light_pos,
                                   const lm::vec3& menu_eye,
                                   const lm::vec3& menu_fog) noexcept {
        if (player_entity_index_count == 0u) return;
        if (player_entity_model_index == static_cast<io::usize>(-1) || player_entity_model_index >= model_data.models.size()) return;
        const ge::Modeller::ImportedModel& model_def = model_data.models[player_entity_model_index];
        if (model_def.kind != ge::Modeller::ModelKind::Entity) return;

        const float w = width() > 0 ? static_cast<float>(width()) : 1.f;
        const float h = height() > 0 ? static_cast<float>(height()) : 1.f;
        const float nx = clampf((mouseX() / w) * 2.f - 1.f, -1.f, 1.f);
        const float ny = clampf((mouseY() / h) * 2.f - 1.f, -1.f, 1.f);

        const float menu_anim_time = frame.scene_time * 0.2f; // 5x slower menu character animation

        io::u32 clip_index = player_clip_walk;
        if (player_clip_run != ENTITY_CLIP_INVALID && lm::sinf(menu_anim_time * 0.55f) > 0.10f)
            clip_index = player_clip_run;
        if (clip_index == ENTITY_CLIP_INVALID) clip_index = player_clip_still;
        if (clip_index == ENTITY_CLIP_INVALID && !model_def.clips.empty()) clip_index = 0u;
        if (clip_index == ENTITY_CLIP_INVALID) return;

        const float look_yaw = clampf(nx * 52.f, -58.f, 58.f);
        const float torso_yaw = clampf(look_yaw * 0.42f, -24.f, 24.f);
        const float head_yaw = clampf(look_yaw - torso_yaw, -85.f, 85.f);
        const float head_pitch = clampf(-ny * 32.f, -35.f, 35.f);
        const float clip_speed = (clip_index == player_clip_run) ? 1.55f : 1.10f;
        const float clip_time = menu_anim_time * clip_speed;

        UpdateEntityAnimatorForClip(player_entity_model_index, clip_index, clip_time,
                                    player_torso_bone_index, torso_yaw, 0.f,
                                    player_head_bone_index, head_yaw, head_pitch,
                                    false);

        UseEntityPass(view, proj, light_pos, menu_eye, 1.f, menu_fog);
        if (entity_uniforms.u_bones0 >= 0 && player_entity_bone_count > 0u)
            gl::UniformMatrix4fv(entity_uniforms.u_bones0, static_cast<int>(player_entity_bone_count), false, entity_bones[0][0].data());

        const float feet_y = -0.32f + lm::sinf(menu_anim_time * 2.2f) * 0.02f;
        const lm::mat4 model = lm::mat4_translate(2.f, feet_y, -0.85f)
            * lm::mat4_rotate_y(lm::radians(-90.f))
            * lm::mat4_scale(1.06f, 1.06f, 1.06f);
        DrawEntityMesh(model, player_entity_vao, player_entity_ebo, player_entity_index_count);
    }

    struct SkyCycleState {
        float time_sec = 0.f;
        float day_length_sec = 2100.f; // full cycle seconds
        float day_phase = 0.f;         // [0..1)
        float daylight = 1.f;          // [0..1]
        float eclipse = 0.f;           // [0..1]
        lm::vec3 sun_dir{ 0.f, 1.f, 0.f };
        lm::vec3 fog_color{ 0.70f, 0.80f, 0.92f };
    };

    IO_NODISCARD inline SkyCycleState ComputeSkyCycle(io::u64 now_ms) const noexcept {
        SkyCycleState s{};
        io::u32 day_ms = 1200000u;   // default: 20 min
        io::u32 night_ms = 900000u;  // default: 15 min
        io::u32 phase_base_ms = 0u;
        io::u64 sync_local_ms = now_ms;
        bool synced = false;

        net_world_time_lock.lock();
        synced = net_world_time_synced;
        phase_base_ms = net_world_phase_ms;
        day_ms = net_world_day_ms;
        night_ms = net_world_night_ms;
        sync_local_ms = net_world_sync_local_ms;
        net_world_time_lock.unlock();

        if (day_ms < 1000u) day_ms = 1000u;
        if (night_ms < 1000u) night_ms = 1000u;
        io::u32 cycle_ms = day_ms + night_ms;
        if (cycle_ms == 0u) cycle_ms = 1u;

        io::u32 phase_ms = 0u;
        io::u32 day_index = 0u;
        if (synced) {
            const io::u64 elapsed_ms = (now_ms >= sync_local_ms) ? (now_ms - sync_local_ms) : 0u;
            const io::u64 total_ms = elapsed_ms + static_cast<io::u64>(phase_base_ms);
            day_index = io::div_u64_u32(total_ms, cycle_ms, &phase_ms);
        } else {
            const io::u64 elapsed_ms = (now_ms >= world_time_start_ms) ? (now_ms - world_time_start_ms) : 0u;
            day_index = io::div_u64_u32(elapsed_ms, cycle_ms, &phase_ms);
        }

        s.day_length_sec = static_cast<float>(cycle_ms) * 0.001f;
        s.time_sec = static_cast<float>(day_index) * s.day_length_sec + static_cast<float>(phase_ms) * 0.001f;
        s.day_phase = static_cast<float>(phase_ms) / static_cast<float>(cycle_ms);

        const float angle = s.day_phase * 6.28318530718f;
        s.sun_dir = lm::vec3_norm({ lm::cosf(angle), lm::sinf(angle), 0.28f });

        const float day_ratio = static_cast<float>(day_ms) / static_cast<float>(cycle_ms);
        const float sun_elev = s.sun_dir[1];
        // Threshold chosen from configured day/night ratio, then smoothed.
        const float threshold = lm::sinf((0.5f - day_ratio) * 3.14159265359f);
        const float twilight = 0.085f;
        float day_factor = clampf((sun_elev - (threshold - twilight)) / (2.f * twilight), 0.f, 1.f);
        day_factor = day_factor * day_factor * (3.f - 2.f * day_factor);

        float sun_curve = clampf((sun_elev - threshold) / (1.f - threshold), 0.f, 1.f);
        sun_curve = sun_curve * sun_curve * (3.f - 2.f * sun_curve);
        float daylight = 0.06f + day_factor * (0.16f + 0.78f * sun_curve);
        daylight = clampf(daylight, 0.f, 1.f);

        float day_t = 0.f;
        if (phase_ms < day_ms)
            day_t = static_cast<float>(phase_ms) / static_cast<float>(day_ms);

        // Every 7 game days: one eclipse window around noon.
        s.eclipse = 0.f;
        if (phase_ms < day_ms && (day_index % 7u) == 0u) {
            float noon_dist = day_t - 0.5f;
            if (noon_dist < 0.f) noon_dist = -noon_dist;
            const float eclipse_window = clampf(1.f - noon_dist / 0.075f, 0.f, 1.f);
            s.eclipse = eclipse_window * eclipse_window;
        }

        s.daylight = clampf(daylight * (1.f - 0.85f * s.eclipse), 0.f, 1.f);

        const lm::vec3 day_fog{ 0.70f, 0.80f, 0.92f };
        const lm::vec3 night_fog{ 0.02f, 0.03f, 0.07f };
        s.fog_color = night_fog * (1.f - s.daylight) + day_fog * s.daylight;
        return s;
    }

    inline void DrawSkyPass(const SkyCycleState& sky, float aspect) noexcept {
        const float h = height() > 0 ? static_cast<float>(height()) : 1.f;
        const float w = width() > 0 ? static_cast<float>(width()) : 1.f;
        const float tan_half = lm::tanf(lm::radians(camera.fov * 0.5f));

        sky_shader.Use();
        if (sky_uniforms.u_screen >= 0) gl::Uniform2f(sky_uniforms.u_screen, w, h);
        if (sky_uniforms.u_cam_forward_x >= 0) gl::Uniform1f(sky_uniforms.u_cam_forward_x, camera.front[0]);
        if (sky_uniforms.u_cam_forward_y >= 0) gl::Uniform1f(sky_uniforms.u_cam_forward_y, camera.front[1]);
        if (sky_uniforms.u_cam_forward_z >= 0) gl::Uniform1f(sky_uniforms.u_cam_forward_z, camera.front[2]);
        if (sky_uniforms.u_cam_right_x >= 0) gl::Uniform1f(sky_uniforms.u_cam_right_x, camera.right[0]);
        if (sky_uniforms.u_cam_right_y >= 0) gl::Uniform1f(sky_uniforms.u_cam_right_y, camera.right[1]);
        if (sky_uniforms.u_cam_right_z >= 0) gl::Uniform1f(sky_uniforms.u_cam_right_z, camera.right[2]);
        if (sky_uniforms.u_cam_up_x >= 0) gl::Uniform1f(sky_uniforms.u_cam_up_x, camera.up[0]);
        if (sky_uniforms.u_cam_up_y >= 0) gl::Uniform1f(sky_uniforms.u_cam_up_y, camera.up[1]);
        if (sky_uniforms.u_cam_up_z >= 0) gl::Uniform1f(sky_uniforms.u_cam_up_z, camera.up[2]);
        if (sky_uniforms.u_aspect >= 0) gl::Uniform1f(sky_uniforms.u_aspect, aspect);
        if (sky_uniforms.u_tan_half_fov >= 0) gl::Uniform1f(sky_uniforms.u_tan_half_fov, tan_half);
        if (sky_uniforms.u_time_sec >= 0) gl::Uniform1f(sky_uniforms.u_time_sec, sky.time_sec);
        if (sky_uniforms.u_day_length_sec >= 0) gl::Uniform1f(sky_uniforms.u_day_length_sec, sky.day_length_sec);
        if (sky_uniforms.u_daylight >= 0) gl::Uniform1f(sky_uniforms.u_daylight, sky.daylight);
        if (sky_uniforms.u_eclipse >= 0) gl::Uniform1f(sky_uniforms.u_eclipse, sky.eclipse);
        float region_mana = 1.f;
        float region_instability = 0.f;
        float region_decay = 0.f;
        GetActiveRegionVisual(region_mana, region_instability, region_decay);
        if (sky_uniforms.u_region_mana >= 0) gl::Uniform1f(sky_uniforms.u_region_mana, region_mana);
        if (sky_uniforms.u_region_instability >= 0) gl::Uniform1f(sky_uniforms.u_region_instability, region_instability);
        if (sky_uniforms.u_region_decay >= 0) gl::Uniform1f(sky_uniforms.u_region_decay, region_decay);

        sky_vao.bind();
        gl::DrawArrays(gl::PrimitiveMode::Triangles, 0, 3);
    }

    IO_NODISCARD static inline io::u32 TargetBreakCrackStage(float progress) noexcept {
        if (progress <= 0.f) return 0u;
        io::i32 stage = static_cast<io::i32>(progress * 5.f) + 1;
        if (stage < 1) stage = 1;
        if (stage > 5) stage = 5;
        return static_cast<io::u32>(stage);
    }

    inline void DrawTargetBreakCrackOverlay(const lm::vec3& camera_world) noexcept {
        if (!target_block_valid || item_sprite_index_count == 0u) return;
        const io::u32 stage = TargetBreakCrackStage(target_break_progress);
        if (stage == 0u) return;
        if (stage > 5u) return;
        const BlockFaceUv uv = break_crack_uv[stage - 1u];
        if (!uv.valid) return;

        const float x0 = static_cast<float>(target_block_wx) - camera_world[0];
        const float y0 = static_cast<float>(target_block_wy) - camera_world[1];
        const float z0 = static_cast<float>(target_block_wz) - camera_world[2];
        const float x1 = x0 + 1.f;
        const float y1 = y0 + 1.f;
        const float z1 = z0 + 1.f;
        const float eps = 0.0015f;

        terrain_shader.Use();
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        if (terrain_uniforms.u_atlas >= 0)
            gl::Uniform1i(terrain_uniforms.u_atlas, 0);
        if (terrain_uniforms.u_chunk_tiled_mode >= 0)
            gl::Uniform1i(terrain_uniforms.u_chunk_tiled_mode, 0);
        if (terrain_uniforms.u_model >= 0) {
            const lm::mat4 identity = lm::mat4_identity();
            gl::UniformMatrix4fv(terrain_uniforms.u_model, 1, false, identity[0].data());
        }

        const gl::Polygon prev_poly = frame.wireframe_mode ? gl::Polygon::Line : gl::Polygon::Fill;
        gl::PolygonMode(gl::Face::FrontAndBack, gl::Polygon::Fill);
        gl::Enable(gl::Capability::Blend);
        gl::BlendFunc(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha);
        ::glDepthMask(FALSE);

        item_sprite_vao.bind();
        item_sprite_ebo.bind();
        DrawHeldItemQuad(lm::vec3{ x1 + eps, y0, z0 }, lm::vec3{ x1 + eps, y0, z1 }, lm::vec3{ x1 + eps, y1, z1 }, lm::vec3{ x1 + eps, y1, z0 },
                         uv.u0, uv.v0, uv.u1, uv.v1);
        DrawHeldItemQuad(lm::vec3{ x0 - eps, y0, z1 }, lm::vec3{ x0 - eps, y0, z0 }, lm::vec3{ x0 - eps, y1, z0 }, lm::vec3{ x0 - eps, y1, z1 },
                         uv.u0, uv.v0, uv.u1, uv.v1);
        DrawHeldItemQuad(lm::vec3{ x0, y1 + eps, z0 }, lm::vec3{ x1, y1 + eps, z0 }, lm::vec3{ x1, y1 + eps, z1 }, lm::vec3{ x0, y1 + eps, z1 },
                         uv.u0, uv.v0, uv.u1, uv.v1);
        DrawHeldItemQuad(lm::vec3{ x0, y0 - eps, z1 }, lm::vec3{ x1, y0 - eps, z1 }, lm::vec3{ x1, y0 - eps, z0 }, lm::vec3{ x0, y0 - eps, z0 },
                         uv.u0, uv.v0, uv.u1, uv.v1);
        DrawHeldItemQuad(lm::vec3{ x0, y0, z1 + eps }, lm::vec3{ x1, y0, z1 + eps }, lm::vec3{ x1, y1, z1 + eps }, lm::vec3{ x0, y1, z1 + eps },
                         uv.u0, uv.v0, uv.u1, uv.v1);
        DrawHeldItemQuad(lm::vec3{ x1, y0, z0 - eps }, lm::vec3{ x0, y0, z0 - eps }, lm::vec3{ x0, y1, z0 - eps }, lm::vec3{ x1, y1, z0 - eps },
                         uv.u0, uv.v0, uv.u1, uv.v1);

        ::glDepthMask(TRUE);
        gl::Disable(gl::Capability::Blend);
        gl::PolygonMode(gl::Face::FrontAndBack, prev_poly);
    }

    inline void DrawTargetBlockOverlay(const lm::mat4& view, const lm::mat4& proj,
                                       const lm::vec3& camera_world) noexcept {
        if (!target_block_valid || draw_index_count == 0u) return;
        DrawTargetBreakCrackOverlay(camera_world);
        const float rel_x = static_cast<float>(target_block_wx) + 0.5f - camera_world[0];
        const float rel_y = static_cast<float>(target_block_wy) + 0.5f - camera_world[1];
        const float rel_z = static_cast<float>(target_block_wz) + 0.5f - camera_world[2];
        const float scale = 1.02f + target_break_progress * 0.10f;
        const lm::mat4 model = lm::mat4_translate(rel_x, rel_y, rel_z) * lm::mat4_scale(scale, scale, scale);
        const lm::mat4 mvp = proj * (view * model);
        highlight_shader.Use();
        if (highlight_u_mvp >= 0)
            gl::UniformMatrix4fv(highlight_u_mvp, 1, false, mvp[0].data());
        if (highlight_u_alpha >= 0)
            gl::Uniform1f(highlight_u_alpha, 0.40f + target_break_progress * 0.45f);
        gl::Enable(gl::Capability::Blend);
        gl::BlendFunc(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha);
        gl::PolygonMode(gl::Face::FrontAndBack, gl::Polygon::Line);
        vao.bind();
        ebo.bind();
        gl::DrawElements(gl::PrimitiveMode::Triangles, static_cast<int>(draw_index_count),
                         gl::DrawElementsType::UnsignedInt, nullptr);
        gl::PolygonMode(gl::Face::FrontAndBack, frame.wireframe_mode ? gl::Polygon::Line : gl::Polygon::Fill);
        gl::Disable(gl::Capability::Blend);
    }

    inline void DrawRegionBoundaryOverlay(const lm::mat4& view, const lm::mat4& proj,
                                          const lm::vec3& camera_world) noexcept {
        if (!dev_hud_visible) return;
        if (highlight_u_mvp < 0 || highlight_u_alpha < 0) return;

        static constexpr io::u32 REGION_LINE_VERT_CAP = 4096u;
        lm::vec3 line_vertices[REGION_LINE_VERT_CAP]{};
        io::u32 line_count = 0u;

        const io::i32 wx_center = floor_to_i32(camera.position[0]);
        const io::i32 wy_center = floor_to_i32(camera.position[1]);
        const io::i32 wz_center = floor_to_i32(camera.position[2]);

        io::i32 radius = static_cast<io::i32>(render_distance_chunks * ge::voxel::CHUNK_SIZE);
        const io::i32 min_radius = ge::region::VORONOI_CELL_SIZE / 2;
        const io::i32 max_radius = ge::region::VORONOI_CELL_SIZE * 2;
        if (radius < min_radius) radius = min_radius;
        if (radius > max_radius) radius = max_radius;

        io::i32 step = ge::region::VORONOI_CELL_SIZE / 24;
        if (step < 12) step = 12;
        if (step > 32) step = 32;

        const io::i32 x0 = ge::region::floor_div_i32(wx_center - radius, step) * step;
        const io::i32 z0 = ge::region::floor_div_i32(wz_center - radius, step) * step;
        const io::i32 x1 = wx_center + radius;
        const io::i32 z1 = wz_center + radius;

        const float y_lo = static_cast<float>(wy_center - 20) - camera_world[1];
        const float y_hi = static_cast<float>(wy_center + 28) - camera_world[1];

        auto push_segment = [&](float x, float z) noexcept {
            if (line_count + 2u > REGION_LINE_VERT_CAP) return;
            line_vertices[line_count++] = lm::vec3{ x - camera_world[0], y_lo, z - camera_world[2] };
            line_vertices[line_count++] = lm::vec3{ x - camera_world[0], y_hi, z - camera_world[2] };
        };

        for (io::i32 z = z0; z <= z1; z += step) {
            for (io::i32 x = x0; x <= x1; x += step) {
                const ge::region::RegionId id_here = ge::region::region_id_from_world(x, wy_center, z);

                if (x + step <= x1) {
                    const ge::region::RegionId id_x = ge::region::region_id_from_world(x + step, wy_center, z);
                    if (id_here != id_x)
                        push_segment(static_cast<float>(x + step / 2), static_cast<float>(z));
                }
                if (z + step <= z1) {
                    const ge::region::RegionId id_z = ge::region::region_id_from_world(x, wy_center, z + step);
                    if (id_here != id_z)
                        push_segment(static_cast<float>(x), static_cast<float>(z + step / 2));
                }
            }
        }

        if (line_count == 0u) return;
        region_line_vbo.bind();
        region_line_vbo.data(line_vertices,
                             static_cast<io::usize>(line_count) * sizeof(lm::vec3),
                             gl::BufferUsage::DynamicDraw);
        region_line_vertex_count = line_count;

        const lm::mat4 mvp = proj * view;
        highlight_shader.Use();
        gl::UniformMatrix4fv(highlight_u_mvp, 1, false, mvp[0].data());
        gl::Uniform1f(highlight_u_alpha, 0.72f);
        gl::Enable(gl::Capability::Blend);
        gl::BlendFunc(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha);
        ::glDepthMask(FALSE);
        region_line_vao.bind();
        gl::DrawArrays(gl::PrimitiveMode::Lines, 0, static_cast<int>(region_line_vertex_count));
        ::glDepthMask(TRUE);
        gl::Disable(gl::Capability::Blend);
    }

    inline void RenderScene() noexcept {
        const float h = height() > 0 ? static_cast<float>(height()) : 1.0f;
        const float aspect = static_cast<float>(width()) / h;
        const gl::Polygon world_poly = frame.wireframe_mode ? gl::Polygon::Line : gl::Polygon::Fill;
        gl::PolygonMode(gl::Face::FrontAndBack, gl::Polygon::Fill);

        if (screen == ScreenState::InGame) {
            head_overlay_active = false;
            head_overlay_block = ge::voxel::BlockId::Air;
            const bool use_oit = EnsureLiquidOitBuffers();
            if (use_oit) {
                gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, liquid_scene_fbo);
                gl::Viewport(0, 0, static_cast<int>(liquid_oit_w), static_cast<int>(liquid_oit_h));
            } else {
                gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, 0u);
                gl::Viewport(0, 0, width(), height());
            }

            const SkyCycleState sky = ComputeSkyCycle(io::monotonic_ms());
            gl::ClearColor(sky.fog_color[0], sky.fog_color[1], sky.fog_color[2], 1.0f);
            gl::Clear(gl::buffer_bit.Color | gl::buffer_bit.Depth);
            gl::Disable(gl::Capability::DepthTest);
            DrawSkyPass(sky, aspect);
            gl::Enable(gl::Capability::DepthTest);

            const lm::mat4 view = lm::mat4_look_at(lm::vec3{ 0.f, 0.f, 0.f }, camera.front, camera.up);
            const float rx = static_cast<float>(render_distance_chunks * ge::voxel::CHUNK_SIZE);
            const float ry = static_cast<float>(WorldYRadiusChunks() * ge::voxel::CHUNK_SIZE);
            const float rz = static_cast<float>(render_distance_chunks * ge::voxel::CHUNK_SIZE);
            float far_plane = lm::sqrtf(rx * rx + ry * ry + rz * rz) + 96.f;
            if (far_plane < 420.f) far_plane = 420.f;
            const lm::mat4 proj = camera.projection_matrix(aspect, 0.1f, far_plane);
            const lm::vec3 light_world = sky.sun_dir * 220.f;
            lm::vec3 camera_world = camera.position;
            camera_world[1] -= player_sneak_view_offset;
            UpdateTargetBlockState(frame.last_dt);
            const lm::vec3 light_pos{
                light_world[0] - camera_world[0],
                light_world[1] - camera_world[1],
                light_world[2] - camera_world[2]
            };
            UpdateChunkPipeline(camera.position);
            gl::PolygonMode(gl::Face::FrontAndBack, world_poly);
            UseTerrainPass(view, proj, light_pos, lm::vec3{ 0.f, 0.f, 0.f }, sky.sun_dir, sky.daylight, sky.fog_color);
            DrawChunkWorld(view, proj, camera_world, CHUNK_RENDER_MASK_SOLID, terrain_uniforms.u_model);
            DrawSandLerpVisuals(io::monotonic_ms(), camera_world);
            DrawWorldItemActors(camera_world);
            DrawTargetBlockOverlay(view, proj, camera_world);
            DrawRegionBoundaryOverlay(view, proj, camera_world);
            DrawFirstPersonHeldItem(camera_world);

            if (entity_index_count > 0u || player_entity_index_count > 0u) {
                UseEntityPass(view, proj, light_pos, lm::vec3{ 0.f, 0.f, 0.f }, sky.daylight, sky.fog_color);
                if (entity_index_count > 0u)
                    DrawWorldActors(camera_world, frame.last_dt);
                if (player_entity_index_count > 0u)
                    DrawRemotePlayers(camera_world, frame.last_dt);
            }

            gl::Enable(gl::Capability::Blend);
            ::glDepthMask(FALSE);
            gl::Enable(gl::Capability::CullFace);
            gl::CullFace(gl::Face::Back);

            if (use_oit) {
                gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, liquid_accum_fbo);
                gl::ClearColor(0.f, 0.f, 0.f, 0.f);
                gl::Clear(gl::buffer_bit.Color);
                gl::BlendFunc(gl::BlendFactor::One, gl::BlendFactor::One);
                UseLiquidPass(liquid_shader, liquid_uniforms, view, proj, lm::vec3{ 0.f, 0.f, 0.f }, sky.sun_dir, sky.daylight, sky.fog_color,
                              ::ge::LIQUID_TRANSPARENCY, 4.5f, 0.35f, 0.20f, 0.0f, 0);
                DrawChunkWorld(view, proj, camera_world, CHUNK_RENDER_MASK_LIQUID, liquid_uniforms.u_model);

                gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, liquid_reveal_fbo);
                gl::ClearColor(1.f, 1.f, 1.f, 1.f);
                gl::Clear(gl::buffer_bit.Color);
                gl::BlendFunc(gl::BlendFactor::Zero, gl::BlendFactor::OneMinusSrcAlpha);
                UseLiquidPass(liquid_shader, liquid_uniforms, view, proj, lm::vec3{ 0.f, 0.f, 0.f }, sky.sun_dir, sky.daylight, sky.fog_color,
                              ::ge::LIQUID_TRANSPARENCY, 4.5f, 0.35f, 0.20f, 0.0f, 1);
                DrawChunkWorld(view, proj, camera_world, CHUNK_RENDER_MASK_LIQUID, liquid_uniforms.u_model);
            } else {
                gl::BlendFunc(gl::BlendFactor::SrcAlpha, gl::BlendFactor::OneMinusSrcAlpha);
                UseLiquidPass(liquid_shader, liquid_uniforms, view, proj, lm::vec3{ 0.f, 0.f, 0.f }, sky.sun_dir, sky.daylight, sky.fog_color,
                              ::ge::LIQUID_TRANSPARENCY, 4.5f, 0.35f, 0.20f, 0.0f, -1);
                DrawChunkWorld(view, proj, camera_world, CHUNK_RENDER_MASK_LIQUID, liquid_uniforms.u_model);
            }

            gl::Disable(gl::Capability::CullFace);
            ::glDepthMask(TRUE);
            gl::Disable(gl::Capability::Blend);

            if (use_oit) {
                gl::PolygonMode(gl::Face::FrontAndBack, gl::Polygon::Fill);
                gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, 0u);
                gl::Disable(gl::Capability::DepthTest);
                liquid_composite_shader.Use();
                gl::ActiveTexture(gl::TexUnit::_0);
                gl::BindTexture(gl::TexTarget::Tex2D, liquid_scene_color_tex);
                gl::ActiveTexture(gl::TexUnit::_1);
                gl::BindTexture(gl::TexTarget::Tex2D, liquid_accum_tex);
                gl::ActiveTexture(gl::TexUnit::_2);
                gl::BindTexture(gl::TexTarget::Tex2D, liquid_reveal_tex);
                if (liquid_composite_uniforms.u_scene >= 0) gl::Uniform1i(liquid_composite_uniforms.u_scene, 0);
                if (liquid_composite_uniforms.u_accum >= 0) gl::Uniform1i(liquid_composite_uniforms.u_accum, 1);
                if (liquid_composite_uniforms.u_reveal >= 0) gl::Uniform1i(liquid_composite_uniforms.u_reveal, 2);
                if (liquid_composite_uniforms.u_single_alpha >= 0) gl::Uniform1f(liquid_composite_uniforms.u_single_alpha, ::ge::LIQUID_TRANSPARENCY);
                sky_vao.bind();
                gl::DrawArrays(gl::PrimitiveMode::Triangles, 0, 3);
                gl::Enable(gl::Capability::DepthTest);
            }

            ge::voxel::BlockId head_block = ge::voxel::BlockId::Air;
            if (TryGetHeadCollisionBlockAt(camera.position, head_block)) {
                head_overlay_active = true;
                head_overlay_block = head_block;
            }
            return;
        }
        else {
            gl::ClearColor(0.f, 0.f, 0.f, 1.0f);
            gl::Clear(gl::buffer_bit.Color | gl::buffer_bit.Depth);
        }

        const float menu_w = width() > 0 ? static_cast<float>(width()) : 1.f;
        const float menu_h = height() > 0 ? static_cast<float>(height()) : 1.f;
        const float menu_nx = clampf((mouseX() / menu_w) * 2.f - 1.f, -1.f, 1.f);
        const float menu_ny = clampf((mouseY() / menu_h) * 2.f - 1.f, -1.f, 1.f);

        const lm::vec3 menu_eye{ menu_nx * 0.22f, 1.15f - menu_ny * 0.08f, 6.2f };
        const lm::vec3 menu_center{ menu_nx * 0.65f, -menu_ny * 0.35f, -0.8f };
        const lm::mat4 view = lm::mat4_look_at(menu_eye, menu_center, { 0.f, 1.f, 0.f });
        const lm::mat4 proj = camera.projection_matrix(aspect);
        const lm::vec3 light_pos{
            3.2f + lm::sinf(frame.scene_time * 0.47f) * 1.6f,
            2.8f + lm::cosf(frame.scene_time * 0.63f) * 0.7f,
            3.5f
        };
        const lm::vec3 menu_sun = lm::vec3_norm(light_pos);
        const lm::vec3 menu_fog{ 0.70f, 0.80f, 0.92f };

        UseTerrainPass(view, proj, light_pos, menu_eye, menu_sun, 1.f, menu_fog);
        static constexpr lm::vec3 menu_stone_offsets[10]{
            { -0.56f,  0.00f, -0.18f },
            {  0.48f, -0.05f,  0.16f },
            { -0.22f,  0.08f,  0.52f },
            {  0.21f,  0.03f, -0.55f },
            { -0.62f,  0.11f,  0.36f },
            {  0.66f, -0.08f, -0.31f },
            { -0.16f,  0.15f, -0.64f },
            {  0.09f, -0.07f,  0.67f },
            { -0.73f,  0.02f, -0.02f },
            {  0.74f,  0.05f,  0.02f }
        };
        const io::usize menu_cube_count = sizeof(menu_cubes) / sizeof(menu_cubes[0]);
        for (io::usize i = 0; i < sizeof(menu_cubes) / sizeof(menu_cubes[0]); ++i) {
            const SceneCube& c = menu_cubes[i];
            const float rx = frame.scene_time * c.rx_speed;
            const float ry = frame.scene_time * c.ry_speed;
            const float rz = frame.scene_time * c.rz_speed;
            DrawMenuCube(ge::voxel::BlockId::Grass, ModelMatrix(c.x, c.y, c.z, rx, ry, rz));
            DrawMenuCube(ge::voxel::BlockId::Dirt, ModelMatrix(c.x, c.y - 1.00f, c.z,
                                                               rx * 0.86f, ry * 0.92f, rz * 0.88f));
            DrawMenuCube(ge::voxel::BlockId::Dirt, ModelMatrix(c.x, c.y - 2.00f, c.z,
                                                               rx * 0.72f, ry * 0.79f, rz * 0.74f));
        }
        for (io::usize i = 0; i < sizeof(menu_stone_offsets) / sizeof(menu_stone_offsets[0]); ++i) {
            const SceneCube& c = menu_cubes[i % menu_cube_count];
            const lm::vec3 off = menu_stone_offsets[i];
            const float base_spin = frame.scene_time * (0.14f + static_cast<float>(i % 3u) * 0.03f);
            DrawMenuCube(ge::voxel::BlockId::Stone,
                         ModelMatrix(c.x + off[0], c.y - 2.85f + off[1], c.z + off[2],
                                     base_spin * 0.28f, base_spin * 0.44f, base_spin * 0.33f));
        }
        DrawMainMenuPlayer(view, proj, light_pos, menu_eye, menu_fog);
    }

    template<io::usize N>
    inline void WriteSessionContext(io::StackOut<N>& out) const noexcept {
        out << PlayerNameView() << " at ";
        switch (session.mode) {
        case SessionMode::Singleplayer:
            if (session.server_name_len > 0) out << SessionServerNameView();
            else out << "Singleplayer";
            break;
        case SessionMode::Multiplayer:
            if (session.server_name_len > 0) out << SessionServerNameView();
            else out << "Multiplayer";
            if (session.endpoint_len > 0)
                out << " (" << SessionEndpointView() << ")";
            break;
        default:
            out << "Main menu";
            break;
        }
    }


