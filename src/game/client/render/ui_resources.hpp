    inline bool UploadAtlasTexture() noexcept {
        if (!texture_atlas.atlas_pixels || texture_atlas.atlas_width == 0 || texture_atlas.atlas_height == 0) return false;
        if (atlas_tex_gl) {
            gl::DeleteTextures(1, &atlas_tex_gl);
            atlas_tex_gl = 0;
        }

        gl::GenTextures(1, &atlas_tex_gl);
        gl::ActiveTexture(gl::TexUnit::_0);
        gl::BindTexture(gl::TexTarget::Tex2D, atlas_tex_gl);
        gl::PixelStorei(gl::UNPACK_ALIGNMENT, 1);
        gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::MinFilter, static_cast<int>(gl::MinifyingFilter::Nearest));
        gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::MagFilter, static_cast<int>(gl::MagnifyingFilter::Nearest));
        gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::WrapS, static_cast<int>(gl::TexWrap::ClampToEdge));
        gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::WrapT, static_cast<int>(gl::TexWrap::ClampToEdge));
        gl::TexImage2D(gl::TexTarget::Tex2D, 0, gl::InternalFormat::RGBA8,
                       static_cast<int>(texture_atlas.atlas_width), static_cast<int>(texture_atlas.atlas_height), 0,
                       gl::TexFormat::RGBA, gl::DataType::UnsignedByte, texture_atlas.atlas_pixels);
        return atlas_tex_gl != 0;
    }

    inline void DestroyLiquidOitBuffers() noexcept {
        liquid_oit_ready = false;
        liquid_oit_w = 0u;
        liquid_oit_h = 0u;
        if (liquid_scene_fbo) {
            gl::DeleteFramebuffers(1, &liquid_scene_fbo);
            liquid_scene_fbo = 0u;
        }
        if (liquid_accum_fbo) {
            gl::DeleteFramebuffers(1, &liquid_accum_fbo);
            liquid_accum_fbo = 0u;
        }
        if (liquid_reveal_fbo) {
            gl::DeleteFramebuffers(1, &liquid_reveal_fbo);
            liquid_reveal_fbo = 0u;
        }
        if (liquid_scene_color_tex) {
            gl::DeleteTextures(1, &liquid_scene_color_tex);
            liquid_scene_color_tex = 0u;
        }
        if (liquid_accum_tex) {
            gl::DeleteTextures(1, &liquid_accum_tex);
            liquid_accum_tex = 0u;
        }
        if (liquid_reveal_tex) {
            gl::DeleteTextures(1, &liquid_reveal_tex);
            liquid_reveal_tex = 0u;
        }
        if (liquid_depth_rbo) {
            gl::DeleteRenderbuffers(1, &liquid_depth_rbo);
            liquid_depth_rbo = 0u;
        }
    }

    IO_NODISCARD inline bool EnsureLiquidOitBuffers() noexcept {
        if (gl::loaded_major < 3u)
            return false;
        const io::u32 w = width() > 0 ? static_cast<io::u32>(width()) : 1u;
        const io::u32 h = height() > 0 ? static_cast<io::u32>(height()) : 1u;
        if (liquid_oit_ready && liquid_oit_w == w && liquid_oit_h == h)
            return true;

        DestroyLiquidOitBuffers();

        auto create_color_tex = [&](io::u32& tex, gl::InternalFormat internal_fmt, gl::DataType data_type) noexcept -> bool {
            gl::GenTextures(1, &tex);
            if (!tex) return false;
            gl::BindTexture(gl::TexTarget::Tex2D, tex);
            gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::MinFilter, static_cast<int>(gl::MinifyingFilter::Nearest));
            gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::MagFilter, static_cast<int>(gl::MagnifyingFilter::Nearest));
            gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::WrapS, static_cast<int>(gl::TexWrap::ClampToEdge));
            gl::TexParameteri(gl::TexTarget::Tex2D, gl::TexParam::WrapT, static_cast<int>(gl::TexWrap::ClampToEdge));
            gl::TexImage2D(gl::TexTarget::Tex2D, 0, internal_fmt,
                           static_cast<int>(w), static_cast<int>(h), 0,
                           gl::TexFormat::RGBA, data_type, nullptr);
            return true;
        };

        if (!create_color_tex(liquid_scene_color_tex, gl::InternalFormat::RGBA8, gl::DataType::UnsignedByte))
            return false;
        if (!create_color_tex(liquid_accum_tex, gl::InternalFormat::RGBA16_F, gl::DataType::Float))
            return false;
        if (!create_color_tex(liquid_reveal_tex, gl::InternalFormat::RGBA8, gl::DataType::UnsignedByte))
            return false;

        gl::GenRenderbuffers(1, &liquid_depth_rbo);
        if (!liquid_depth_rbo) return false;
        gl::BindRenderbuffer(gl::RenderbufferTarget::Renderbuffer, liquid_depth_rbo);
        gl::RenderbufferStorage(gl::RenderbufferTarget::Renderbuffer, gl::InternalFormat::DEPTH_COMPONENT24,
            static_cast<int>(w), static_cast<int>(h));

        gl::GenFramebuffers(1, &liquid_scene_fbo);
        gl::GenFramebuffers(1, &liquid_accum_fbo);
        gl::GenFramebuffers(1, &liquid_reveal_fbo);
        if (!liquid_scene_fbo || !liquid_accum_fbo || !liquid_reveal_fbo)
            return false;

        gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, liquid_scene_fbo);
        gl::FramebufferTexture2D(gl::FramebufferTarget::Framebuffer, gl::FramebufferAttachment::ColorAttachment0,
                                 gl::TexTarget::Tex2D, liquid_scene_color_tex, 0);
        gl::FramebufferRenderbuffer(gl::FramebufferTarget::Framebuffer, gl::FramebufferAttachment::DepthAttachment,
                                    gl::RenderbufferTarget::Renderbuffer, liquid_depth_rbo);
        if (gl::CheckFramebufferStatus(gl::FramebufferTarget::Framebuffer) != static_cast<io::u32>(gl::FramebufferStatus::Complete))
            return false;

        gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, liquid_accum_fbo);
        gl::FramebufferTexture2D(gl::FramebufferTarget::Framebuffer, gl::FramebufferAttachment::ColorAttachment0,
                                 gl::TexTarget::Tex2D, liquid_accum_tex, 0);
        gl::FramebufferRenderbuffer(gl::FramebufferTarget::Framebuffer, gl::FramebufferAttachment::DepthAttachment,
                                    gl::RenderbufferTarget::Renderbuffer, liquid_depth_rbo);
        if (gl::CheckFramebufferStatus(gl::FramebufferTarget::Framebuffer) != static_cast<io::u32>(gl::FramebufferStatus::Complete))
            return false;

        gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, liquid_reveal_fbo);
        gl::FramebufferTexture2D(gl::FramebufferTarget::Framebuffer, gl::FramebufferAttachment::ColorAttachment0,
                                 gl::TexTarget::Tex2D, liquid_reveal_tex, 0);
        gl::FramebufferRenderbuffer(gl::FramebufferTarget::Framebuffer, gl::FramebufferAttachment::DepthAttachment,
                                    gl::RenderbufferTarget::Renderbuffer, liquid_depth_rbo);
        if (gl::CheckFramebufferStatus(gl::FramebufferTarget::Framebuffer) != static_cast<io::u32>(gl::FramebufferStatus::Complete))
            return false;

        gl::BindRenderbuffer(gl::RenderbufferTarget::Renderbuffer, 0u);
        gl::BindFramebuffer(gl::FramebufferTarget::Framebuffer, 0u);

        liquid_oit_w = w;
        liquid_oit_h = h;
        liquid_oit_ready = true;
        return true;
    }

    inline bool UploadModelMesh(const ge::Modeller::ImportedModel& model) noexcept {
        if (model.vertices.empty() || model.indices.empty()) return false;
        draw_index_count = static_cast<io::u32>(model.indices.size());

        vbo.bind();
        vbo.data(model.vertices.data(),
                 static_cast<io::usize>(model.vertices.size()) * sizeof(ge::Modeller::MeshVertex),
                 gl::BufferUsage::StaticDraw);
        ebo.bind();
        ebo.data(model.indices.data(),
                 static_cast<io::usize>(model.indices.size()) * sizeof(io::u32),
                 gl::BufferUsage::StaticDraw);

        gl::Attr attrs[]{
            gl::AttrOf<float>(3, false),
            gl::AttrOf<float>(3, false),
            gl::AttrOf<float>(2, false),
        };
        gl::MeshBinder::setup(vao, vbo, { attrs, sizeof(attrs) / sizeof(attrs[0]) });
        return true;
    }

    inline bool UploadEntityMeshTo(const ge::Modeller::ImportedModel& model,
                                   gl::Buffer& out_vbo,
                                   gl::Buffer& out_ebo,
                                   gl::VertexArray& out_vao,
                                   io::u32& out_index_count) noexcept {
        if (model.vertices.empty() || model.indices.empty()) return false;
        out_index_count = static_cast<io::u32>(model.indices.size());

        out_vbo.bind();
        out_vbo.data(model.vertices.data(),
                     static_cast<io::usize>(model.vertices.size()) * sizeof(ge::Modeller::MeshVertex),
                     gl::BufferUsage::StaticDraw);
        out_ebo.bind();
        out_ebo.data(model.indices.data(),
                     static_cast<io::usize>(model.indices.size()) * sizeof(io::u32),
                     gl::BufferUsage::StaticDraw);

        out_vao.bind();
        out_vbo.bind();
        out_ebo.bind();
        gl::VertexAttribPointer(0, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::Modeller::MeshVertex)),
                                reinterpret_cast<void*>(0));
        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(1, 3, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::Modeller::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 3u));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(2, 2, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::Modeller::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 6u));
        gl::EnableVertexAttribArray(2);
        gl::VertexAttribPointer(3, 4, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::Modeller::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 8u));
        gl::EnableVertexAttribArray(3);
        gl::VertexAttribPointer(4, 4, gl::DrawElementsType::Float, false,
                                static_cast<int>(sizeof(ge::Modeller::MeshVertex)),
                                reinterpret_cast<void*>(sizeof(float) * 12u));
        gl::EnableVertexAttribArray(4);
        gl::BindVertexArray(0);
        return true;
    }

    inline bool UploadEntityMesh(const ge::Modeller::ImportedModel& model) noexcept {
        return UploadEntityMeshTo(model, entity_vbo, entity_ebo, entity_vao, entity_index_count);
    }

    IO_NODISCARD inline io::u32 FindEntityClipIndex(const ge::Modeller::ImportedModel& model,
                                                     io::char_view preferred_name) const noexcept {
        if (model.clips.empty()) return ENTITY_CLIP_INVALID;
        for (io::u32 i = 0; i < model.clips.size(); ++i)
            if (ChatCommandEq(model.clips[i].name.as_view(), preferred_name))
                return i;
        for (io::u32 i = 0; i < model.clips.size(); ++i) {
            const io::char_view clip_name = model.clips[i].name.as_view();
            for (io::usize p = clip_name.size(); p > 0u; --p) {
                if (clip_name[p - 1u] != '.') continue;
                const io::char_view tail = clip_name.slice(p, clip_name.size() - p);
                if (ChatCommandEq(tail, preferred_name))
                    return i;
                break;
            }
        }
        return ENTITY_CLIP_INVALID;
    }

    inline bool LoadBlocksAndAtlas() noexcept {
        for (io::usize i = 0; i < FIXED_MODEL_SLOT_COUNT; ++i)
            fixed_model_index_cache[i] = static_cast<io::usize>(-1);

        ge::ResourceManager::TextureAtlasOptions atlas_opt{};
        atlas_opt.desired_channels = 4;
        atlas_opt.atlas_padding_px = 1;
        atlas_opt.max_blocks = 512;
        if (!ge::ResourceManager::texture_atlas_from(texture_atlas, atlas_opt)) return false;
        InvalidatePseudo3dSeamUvCache();

        ge::Modeller::PlanResult plan{};
        if (!ge::Modeller::Plan(plan)) return false;
        if (!ge::Modeller::Build(plan, texture_atlas, model_data)) return false;

        const io::view<const ge::ResourceManager::BlockDesc> blocks = ge::Modeller::BlockDescs(model_data);
        if (blocks.empty()) return false;
        if (!ge::ResourceManager::register_blocks(texture_atlas, blocks)) return false;
        BuildBlockFaceUvTable();
        BuildBlockMapTintTable();
        BuildBreakCrackUvTable();

        for (io::usize i = 0; i < FIXED_MODEL_SLOT_COUNT; ++i)
            fixed_model_index_cache[i] = model_data.fixed_model_indices[i];

        const io::usize book_index = fixed_model_index_cache[static_cast<io::usize>(ge::Modeller::FixedModelSlot::LevitatingBook)];
        const ge::Modeller::ImportedModel* book = nullptr;
        if (book_index != static_cast<io::usize>(-1) && book_index < model_data.models.size())
            book = &model_data.models[book_index];
        const io::usize player_index = fixed_model_index_cache[static_cast<io::usize>(ge::Modeller::FixedModelSlot::Player)];
        const ge::Modeller::ImportedModel* player_model = nullptr;
        if (player_index != static_cast<io::usize>(-1) && player_index < model_data.models.size())
            player_model = &model_data.models[player_index];
        entity_model_index = static_cast<io::usize>(-1);
        entity_index_count = 0u;
        entity_bone_count = 0u;
        player_entity_model_index = static_cast<io::usize>(-1);
        player_entity_index_count = 0u;
        player_entity_bone_count = 0u;
        player_head_bone_index = -1;
        player_torso_bone_index = -1;
        player_right_arm_bone_index = -1;
        player_left_leg_bone_index = -1;
        player_right_leg_bone_index = -1;
        player_clip_still = ENTITY_CLIP_INVALID;
        player_clip_walk = ENTITY_CLIP_INVALID;
        player_clip_run = ENTITY_CLIP_INVALID;
        player_clip_eat = ENTITY_CLIP_INVALID;
        player_clip_crawl = ENTITY_CLIP_INVALID;
        player_clip_crawl_down = ENTITY_CLIP_INVALID;
        player_clip_crawl_up = ENTITY_CLIP_INVALID;
        if (book && book->kind == ge::Modeller::ModelKind::Entity && !book->vertices.empty() && !book->indices.empty()) {
            entity_model_index = book_index;
            entity_bone_count = book->joint_count;
            if (entity_bone_count == 0u) entity_bone_count = 1u;
            if (entity_bone_count > ENTITY_BONE_CAP) entity_bone_count = ENTITY_BONE_CAP;
            for (io::u32 i = 0; i < ENTITY_BONE_CAP; ++i) {
                entity_bones[i] = lm::mat4_identity();
                entity_local_mats[i] = lm::mat4_identity();
                entity_global_mats[i] = lm::mat4_identity();
                entity_pose_t[i] = ge::Modeller::Float3{};
                entity_pose_r[i] = ge::Modeller::Float4{ 0.f, 0.f, 0.f, 1.f };
                entity_pose_s[i] = ge::Modeller::Float3{ 1.f, 1.f, 1.f };
            }
            entity_anim_clip_index = ENTITY_CLIP_INVALID;
            entity_clip_stay = FindEntityClipIndex(*book, "stay");
            entity_clip_levitate = FindEntityClipIndex(*book, "levitate");
            if (entity_clip_levitate == ENTITY_CLIP_INVALID && !book->clips.empty())
                entity_clip_levitate = 0u;
            entity_anim_clip_index = (entity_clip_levitate != ENTITY_CLIP_INVALID) ? entity_clip_levitate : entity_clip_stay;
        }
        if (player_model && player_model->kind == ge::Modeller::ModelKind::Entity &&
            !player_model->vertices.empty() && !player_model->indices.empty()) {
            player_entity_model_index = player_index;
            player_entity_bone_count = player_model->joint_count;
            if (player_entity_bone_count == 0u) player_entity_bone_count = 1u;
            if (player_entity_bone_count > ENTITY_BONE_CAP) player_entity_bone_count = ENTITY_BONE_CAP;
            player_clip_still = FindEntityClipIndex(*player_model, "still");
            player_clip_walk = FindEntityClipIndex(*player_model, "walk");
            player_clip_run = FindEntityClipIndex(*player_model, "run");
            player_clip_eat = FindEntityClipIndex(*player_model, "eat");
            player_clip_crawl = FindEntityClipIndex(*player_model, "crawl");
            player_clip_crawl_down = FindEntityClipIndex(*player_model, "crawl_down");
            player_clip_crawl_up = FindEntityClipIndex(*player_model, "crawl_up");
            io::u32 child_counts[ENTITY_BONE_CAP]{};
            for (io::u32 bi = 0u; bi < player_model->bones.size() && bi < ENTITY_BONE_CAP; ++bi) {
                const io::i32 parent = player_model->bones[bi].parent_bone;
                if (parent >= 0 && static_cast<io::u32>(parent) < ENTITY_BONE_CAP)
                    ++child_counts[static_cast<io::u32>(parent)];
            }

            auto contains_token = [&](io::char_view name, io::char_view token) noexcept -> bool {
                if (name.empty() || token.empty() || name.size() < token.size()) return false;
                for (io::usize i = 0u; i + token.size() <= name.size(); ++i) {
                    bool ok = true;
                    for (io::usize j = 0u; j < token.size(); ++j) {
                        if (ToLowerAscii(name[i + j]) != ToLowerAscii(token[j])) {
                            ok = false;
                            break;
                        }
                    }
                    if (ok) return true;
                }
                return false;
            };

            io::i32 best_head_with_children = -1;
            io::i32 best_head_any = -1;
            io::i32 best_waist = -1;
            io::i32 best_body_group = -1;
            io::i32 best_left_leg_with_children = -1;
            io::i32 best_right_leg_with_children = -1;
            io::i32 best_left_leg_any = -1;
            io::i32 best_right_leg_any = -1;
            io::i32 best_right_arm_with_children = -1;
            io::i32 best_right_arm_any = -1;
            for (io::u32 bi = 0u; bi < player_model->bones.size() && bi < ENTITY_BONE_CAP; ++bi) {
                const io::char_view bn = player_model->bones[bi].name.as_view();
                if (bn.empty()) continue;
                const bool has_children = child_counts[bi] > 0u;
                const bool has_leg = contains_token(bn, "leg");
                const bool has_left = contains_token(bn, "left");
                const bool has_right = contains_token(bn, "right");
                const bool has_arm = contains_token(bn, "arm");
                if (contains_token(bn, "head")) {
                    if (best_head_any < 0) best_head_any = static_cast<io::i32>(bi);
                    if (has_children && best_head_with_children < 0) {
                        best_head_with_children = static_cast<io::i32>(bi);
                    }
                }
                if (best_waist < 0 && contains_token(bn, "waist") && has_children)
                    best_waist = static_cast<io::i32>(bi);
                if (best_body_group < 0 && contains_token(bn, "body") && has_children)
                    best_body_group = static_cast<io::i32>(bi);
                if (has_leg && has_left) {
                    if (best_left_leg_any < 0)
                        best_left_leg_any = static_cast<io::i32>(bi);
                    if (has_children && best_left_leg_with_children < 0)
                        best_left_leg_with_children = static_cast<io::i32>(bi);
                }
                if (has_leg && has_right) {
                    if (best_right_leg_any < 0)
                        best_right_leg_any = static_cast<io::i32>(bi);
                    if (has_children && best_right_leg_with_children < 0)
                        best_right_leg_with_children = static_cast<io::i32>(bi);
                }
                if (has_arm && has_right) {
                    if (best_right_arm_any < 0)
                        best_right_arm_any = static_cast<io::i32>(bi);
                    if (has_children && best_right_arm_with_children < 0)
                        best_right_arm_with_children = static_cast<io::i32>(bi);
                }
            }

            if (best_head_with_children >= 0) player_head_bone_index = best_head_with_children;
            else if (best_head_any >= 0) player_head_bone_index = best_head_any;
            else player_head_bone_index = 0;

            if (best_waist >= 0) player_torso_bone_index = best_waist;
            else if (best_body_group >= 0) player_torso_bone_index = best_body_group;
            else if (player_head_bone_index >= 0 &&
                     static_cast<io::u32>(player_head_bone_index) < player_model->bones.size() &&
                     player_model->bones[static_cast<io::u32>(player_head_bone_index)].parent_bone >= 0)
                player_torso_bone_index = player_model->bones[static_cast<io::u32>(player_head_bone_index)].parent_bone;
            else player_torso_bone_index = player_head_bone_index;

            if (best_left_leg_with_children >= 0) player_left_leg_bone_index = best_left_leg_with_children;
            else player_left_leg_bone_index = best_left_leg_any;
            if (best_right_leg_with_children >= 0) player_right_leg_bone_index = best_right_leg_with_children;
            else player_right_leg_bone_index = best_right_leg_any;
            if (best_right_arm_with_children >= 0) player_right_arm_bone_index = best_right_arm_with_children;
            else player_right_arm_bone_index = best_right_arm_any;
        }

        const bool atlas_ok = UploadAtlasTexture();
        if (atlas_ok)
            RefreshGuiTextureAtlas();
        const bool cube_ok = InitMenuBlockMeshes();
        const bool sand_ok = InitSandLerpMesh();
        const bool sprite_ok = InitItemSpriteMesh();
        const bool region_line_ok = InitRegionLineMesh();
        bool entity_ok = true;
        if (entity_model_index != static_cast<io::usize>(-1))
            entity_ok = UploadEntityMesh(model_data.models[entity_model_index]);
        bool player_entity_ok = true;
        if (player_entity_model_index != static_cast<io::usize>(-1))
            player_entity_ok = UploadEntityMeshTo(model_data.models[player_entity_model_index],
                                                  player_entity_vbo, player_entity_ebo, player_entity_vao,
                                                  player_entity_index_count);

        return atlas_ok && cube_ok && sand_ok && sprite_ok && region_line_ok && entity_ok && player_entity_ok;
    }

    inline bool LoadShaders() noexcept {
        if (!ge::ResourceManager::shader_from("sky.frag", "sky.vert", sky_shader))
            return false;
        if (!ge::ResourceManager::shader_from("terrain.frag", "terrain.vert", terrain_shader))
            return false;
        if (!ge::ResourceManager::shader_from("liquid.frag", "liquid.vert", liquid_shader))
            return false;
        if (!ge::ResourceManager::shader_from("liquid_oit_composite.frag", "liquid_oit_composite.vert", liquid_composite_shader))
            return false;
        if (!ge::ResourceManager::shader_from("entity.frag", "entity.vert", entity_shader))
            return false;
        if (!ge::ResourceManager::shader_from("fill_orange.frag", "fill_orange.vert", highlight_shader))
            return false;
        if (!ge::ResourceManager::shader_from("post_effect.frag", "liquid_oit_composite.vert", post_effect_shader))
            return false;

        sky_uniforms.u_screen = gl::GetUniformLocation(sky_shader.id(), "uScreen");
        sky_uniforms.u_cam_forward_x = gl::GetUniformLocation(sky_shader.id(), "uCamForwardX");
        sky_uniforms.u_cam_forward_y = gl::GetUniformLocation(sky_shader.id(), "uCamForwardY");
        sky_uniforms.u_cam_forward_z = gl::GetUniformLocation(sky_shader.id(), "uCamForwardZ");
        sky_uniforms.u_cam_right_x = gl::GetUniformLocation(sky_shader.id(), "uCamRightX");
        sky_uniforms.u_cam_right_y = gl::GetUniformLocation(sky_shader.id(), "uCamRightY");
        sky_uniforms.u_cam_right_z = gl::GetUniformLocation(sky_shader.id(), "uCamRightZ");
        sky_uniforms.u_cam_up_x = gl::GetUniformLocation(sky_shader.id(), "uCamUpX");
        sky_uniforms.u_cam_up_y = gl::GetUniformLocation(sky_shader.id(), "uCamUpY");
        sky_uniforms.u_cam_up_z = gl::GetUniformLocation(sky_shader.id(), "uCamUpZ");
        sky_uniforms.u_aspect = gl::GetUniformLocation(sky_shader.id(), "uAspect");
        sky_uniforms.u_tan_half_fov = gl::GetUniformLocation(sky_shader.id(), "uTanHalfFov");
        sky_uniforms.u_time_sec = gl::GetUniformLocation(sky_shader.id(), "uTimeSec");
        sky_uniforms.u_day_length_sec = gl::GetUniformLocation(sky_shader.id(), "uDayLengthSec");
        sky_uniforms.u_daylight = gl::GetUniformLocation(sky_shader.id(), "uDaylight");
        sky_uniforms.u_eclipse = gl::GetUniformLocation(sky_shader.id(), "uEclipse");
        sky_uniforms.u_region_mana = gl::GetUniformLocation(sky_shader.id(), "uRegionMana");
        sky_uniforms.u_region_instability = gl::GetUniformLocation(sky_shader.id(), "uRegionInstability");
        sky_uniforms.u_region_decay = gl::GetUniformLocation(sky_shader.id(), "uRegionDecay");
        post_effect_uniforms.u_black_strength = gl::GetUniformLocation(post_effect_shader.id(), "uBlackStrength");
        post_effect_uniforms.u_red_strength = gl::GetUniformLocation(post_effect_shader.id(), "uRedStrength");
        post_effect_uniforms.u_dead_strength = gl::GetUniformLocation(post_effect_shader.id(), "uDeadStrength");
        post_effect_uniforms.u_map_tint_rg = gl::GetUniformLocation(post_effect_shader.id(), "uMapTintRg");
        post_effect_uniforms.u_map_tint_b = gl::GetUniformLocation(post_effect_shader.id(), "uMapTintB");
        post_effect_uniforms.u_map_tint_strength = gl::GetUniformLocation(post_effect_shader.id(), "uMapTintStrength");
        post_effect_uniforms.u_region_decay = gl::GetUniformLocation(post_effect_shader.id(), "uRegionDecay");
        post_effect_uniforms.u_region_instability = gl::GetUniformLocation(post_effect_shader.id(), "uRegionInstability");
        highlight_u_mvp = gl::GetUniformLocation(highlight_shader.id(), "uMvp");
        highlight_u_alpha = gl::GetUniformLocation(highlight_shader.id(), "uAlpha");

        terrain_uniforms.u_model = gl::GetUniformLocation(terrain_shader.id(), "uModel");
        terrain_uniforms.u_view = gl::GetUniformLocation(terrain_shader.id(), "uView");
        terrain_uniforms.u_proj = gl::GetUniformLocation(terrain_shader.id(), "uProj");
        terrain_uniforms.u_light_x = gl::GetUniformLocation(terrain_shader.id(), "uLightPosX");
        terrain_uniforms.u_light_y = gl::GetUniformLocation(terrain_shader.id(), "uLightPosY");
        terrain_uniforms.u_light_z = gl::GetUniformLocation(terrain_shader.id(), "uLightPosZ");
        terrain_uniforms.u_view_x = gl::GetUniformLocation(terrain_shader.id(), "uViewPosX");
        terrain_uniforms.u_view_y = gl::GetUniformLocation(terrain_shader.id(), "uViewPosY");
        terrain_uniforms.u_view_z = gl::GetUniformLocation(terrain_shader.id(), "uViewPosZ");
        terrain_uniforms.u_atlas = gl::GetUniformLocation(terrain_shader.id(), "uAtlas");
        terrain_uniforms.u_chunk_tiled_mode = gl::GetUniformLocation(terrain_shader.id(), "uChunkTiledMode");
        terrain_uniforms.u_atlas_texel = gl::GetUniformLocation(terrain_shader.id(), "uAtlasTexel");
        terrain_uniforms.u_sun_dir_x = gl::GetUniformLocation(terrain_shader.id(), "uSunDirX");
        terrain_uniforms.u_sun_dir_y = gl::GetUniformLocation(terrain_shader.id(), "uSunDirY");
        terrain_uniforms.u_sun_dir_z = gl::GetUniformLocation(terrain_shader.id(), "uSunDirZ");
        terrain_uniforms.u_daylight = gl::GetUniformLocation(terrain_shader.id(), "uDaylight");
        terrain_uniforms.u_fog_r = gl::GetUniformLocation(terrain_shader.id(), "uFogR");
        terrain_uniforms.u_fog_g = gl::GetUniformLocation(terrain_shader.id(), "uFogG");
        terrain_uniforms.u_fog_b = gl::GetUniformLocation(terrain_shader.id(), "uFogB");
        terrain_uniforms.u_fog_start = gl::GetUniformLocation(terrain_shader.id(), "uFogStart");
        terrain_uniforms.u_fog_end = gl::GetUniformLocation(terrain_shader.id(), "uFogEnd");

        auto init_liquid_uniforms = [](io::u32 shader_id, LiquidUniforms& out) noexcept {
            out.u_model = gl::GetUniformLocation(shader_id, "uModel");
            out.u_view = gl::GetUniformLocation(shader_id, "uView");
            out.u_proj = gl::GetUniformLocation(shader_id, "uProj");
            out.u_view_x = gl::GetUniformLocation(shader_id, "uViewPosX");
            out.u_view_y = gl::GetUniformLocation(shader_id, "uViewPosY");
            out.u_view_z = gl::GetUniformLocation(shader_id, "uViewPosZ");
            out.u_atlas = gl::GetUniformLocation(shader_id, "uAtlas");
            out.u_atlas_texel = gl::GetUniformLocation(shader_id, "uAtlasTexel");
            out.u_sun_dir_x = gl::GetUniformLocation(shader_id, "uSunDirX");
            out.u_sun_dir_y = gl::GetUniformLocation(shader_id, "uSunDirY");
            out.u_sun_dir_z = gl::GetUniformLocation(shader_id, "uSunDirZ");
            out.u_daylight = gl::GetUniformLocation(shader_id, "uDaylight");
            out.u_fog_r = gl::GetUniformLocation(shader_id, "uFogR");
            out.u_fog_g = gl::GetUniformLocation(shader_id, "uFogG");
            out.u_fog_b = gl::GetUniformLocation(shader_id, "uFogB");
            out.u_fog_start = gl::GetUniformLocation(shader_id, "uFogStart");
            out.u_fog_end = gl::GetUniformLocation(shader_id, "uFogEnd");
            out.u_base_alpha = gl::GetUniformLocation(shader_id, "uBaseAlpha");
            out.u_fresnel_power = gl::GetUniformLocation(shader_id, "uFresnelPower");
            out.u_fresnel_strength = gl::GetUniformLocation(shader_id, "uFresnelStrength");
            out.u_edge_softness = gl::GetUniformLocation(shader_id, "uEdgeSoftness");
            out.u_edge_strength = gl::GetUniformLocation(shader_id, "uEdgeStrength");
            out.u_oit_pass = gl::GetUniformLocation(shader_id, "uOitPass");
        };
        init_liquid_uniforms(liquid_shader.id(), liquid_uniforms);
        liquid_composite_uniforms.u_scene = gl::GetUniformLocation(liquid_composite_shader.id(), "uScene");
        liquid_composite_uniforms.u_accum = gl::GetUniformLocation(liquid_composite_shader.id(), "uAccum");
        liquid_composite_uniforms.u_reveal = gl::GetUniformLocation(liquid_composite_shader.id(), "uReveal");
        liquid_composite_uniforms.u_single_alpha = gl::GetUniformLocation(liquid_composite_shader.id(), "uSingleAlpha");

        entity_uniforms.u_model = gl::GetUniformLocation(entity_shader.id(), "uModel");
        entity_uniforms.u_view = gl::GetUniformLocation(entity_shader.id(), "uView");
        entity_uniforms.u_proj = gl::GetUniformLocation(entity_shader.id(), "uProj");
        entity_uniforms.u_light_x = gl::GetUniformLocation(entity_shader.id(), "uLightPosX");
        entity_uniforms.u_light_y = gl::GetUniformLocation(entity_shader.id(), "uLightPosY");
        entity_uniforms.u_light_z = gl::GetUniformLocation(entity_shader.id(), "uLightPosZ");
        entity_uniforms.u_view_x = gl::GetUniformLocation(entity_shader.id(), "uViewPosX");
        entity_uniforms.u_view_y = gl::GetUniformLocation(entity_shader.id(), "uViewPosY");
        entity_uniforms.u_view_z = gl::GetUniformLocation(entity_shader.id(), "uViewPosZ");
        entity_uniforms.u_atlas = gl::GetUniformLocation(entity_shader.id(), "uAtlas");
        entity_uniforms.u_bones0 = gl::GetUniformLocation(entity_shader.id(), "uBones[0]");
        entity_uniforms.u_daylight = gl::GetUniformLocation(entity_shader.id(), "uDaylight");
        entity_uniforms.u_fog_r = gl::GetUniformLocation(entity_shader.id(), "uFogR");
        entity_uniforms.u_fog_g = gl::GetUniformLocation(entity_shader.id(), "uFogG");
        entity_uniforms.u_fog_b = gl::GetUniformLocation(entity_shader.id(), "uFogB");

        return sky_uniforms.u_screen >= 0 &&
            highlight_u_mvp >= 0 && highlight_u_alpha >= 0 &&
            terrain_uniforms.u_model >= 0 && terrain_uniforms.u_view >= 0 && terrain_uniforms.u_proj >= 0 &&
            liquid_uniforms.u_model >= 0 && liquid_uniforms.u_view >= 0 && liquid_uniforms.u_proj >= 0 &&
            liquid_composite_uniforms.u_scene >= 0 && liquid_composite_uniforms.u_accum >= 0 && liquid_composite_uniforms.u_reveal >= 0 &&
            liquid_composite_uniforms.u_single_alpha >= 0 &&
            entity_uniforms.u_model >= 0 && entity_uniforms.u_view >= 0 && entity_uniforms.u_proj >= 0;
    }

    inline bool LoadFonts() noexcept {
        hi::FontAtlasDesc desc{};
        desc.mode = hi::FontAtlasMode::SDF;
        desc.pixel_height = static_cast<io::u16>(ge::FONT_PIXEL_HEIGHT);
        desc.spread_px = 2.f;

        hi::FontId font_id{ -1 };
        const io::char_view roots[] = { ge::PATH_RESOURCES, ge::PATH_RESOURCES_ALT_1, ge::PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            io::StackOut<260> ss{};
            ss << roots[i] << ge::PATH_FONTS_TTF << ge::FILENAME_WORLD_FONT;
            font_id = LoadFont(ss.view());
            if (font_id >= 0) break;
        }
        if (font_id < 0) return false;

        world_atlas = GenerateFontAtlas(font_id, desc,
            stbtt_codepoints::Script::Latin,
            stbtt_codepoints::Script::Cyrillic);
        return world_atlas >= 0;
    }


