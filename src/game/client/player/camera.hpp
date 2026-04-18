#pragma once

    static inline float clampf(float value, float lo, float hi) noexcept {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
    }

    inline void MoveCameraWithCollisions(const lm::vec3& move_delta) noexcept {
        const float ax = absf(move_delta[0]);
        const float ay = absf(move_delta[1]);
        const float az = absf(move_delta[2]);
        float max_comp = ax;
        if (ay > max_comp) max_comp = ay;
        if (az > max_comp) max_comp = az;
        if (max_comp <= 0.000001f) return;

        io::u32 steps = 1u;
        for (float remain = max_comp;
                   remain > 0.20f && steps < 32u;
                   remain -= 0.20f
            )
            ++steps;
        if (steps > 32u) steps = 32u;
        const float inv_steps = 1.f / static_cast<float>(steps);
        const lm::vec3 step = move_delta * inv_steps;

        for (io::u32 i = 0; i < steps; ++i) {
            if (step[0] != 0.f) {
                lm::vec3 test = camera.position;
                test[0] += step[0];
                if (!IsPlayerBodyCollidingAt(test)) camera.position[0] = test[0];
                else                                move_velocity[0] = 0.f;
            }
            if (step[1] != 0.f) {
                lm::vec3 test = camera.position;
                test[1] += step[1];
                if (!IsPlayerBodyCollidingAt(test)) camera.position[1] = test[1];
                else                                move_velocity[1] = 0.f;
            }
            if (step[2] != 0.f) {
                lm::vec3 test = camera.position;
                test[2] += step[2];
                if (!IsPlayerBodyCollidingAt(test)) camera.position[2] = test[2];
                else                                move_velocity[2] = 0.f;
            }
        }
    }

    IO_NODISCARD inline bool ResolvePlayerBodyPenetration() noexcept {
        if (!chunk_world_ready) return false;
        if (!IsPlayerBodyCollidingAt(camera.position)) return true;

        static constexpr float STEP_UP = 0.05f;
        static constexpr io::u32 MAX_STEPS = 40u;
        lm::vec3 probe = camera.position;

        for (io::u32 i = 0; i < MAX_STEPS; ++i) {
            probe[1] += STEP_UP;
            if (IsPlayerBodyCollidingAt(probe))
                continue;
            camera.position = probe;
            if (move_velocity[1] < 0.f) move_velocity[1] = 0.f;
            player_grounded = true;
            player_airborne = false;
            player_air_peak_foot_y = PlayerFeetY(camera.position);
            return true;
        }
        return false;
    }

    IO_NODISCARD inline bool TrySetPlayerCrawlMode(bool crawl_mode) noexcept {
        if (player_crawling == crawl_mode) return true;
        const bool old_mode = player_crawling;
        const float feet_y = PlayerFeetYForMode(camera.position, old_mode);
        lm::vec3 target_eye = camera.position;
        target_eye[1] = feet_y + (crawl_mode ? PlayerCrawlEyeToFeet() : PlayerStandEyeToFeet());
        if (chunk_world_ready && IsPlayerBodyCollidingAtForMode(target_eye, crawl_mode))
            return false;
        player_crawling = crawl_mode;
        camera.position = target_eye;
        if (chunk_world_ready) {
            player_grounded = IsPlayerGroundedAtForMode(camera.position, player_crawling);
            player_air_peak_foot_y = PlayerFeetYForMode(camera.position, player_crawling);
        }
        return true;
    }

    inline void UpdateCamera(float dt) noexcept {
        SyncRuntimeFromLocalPlayerEcs();
        bool has_health = false;
        float hp_value = 0.f;
        float hp_damage = 0.f;
        float hp_fall_blocks = 0.f;
        io::u8 hp_hunger = player_hunger;
        io::u8 hp_flags = player_dead ? ge::net::PLAYER_HEALTH_FLAG_DEAD : 0u;
        ge::net::DeathReason hp_death_reason = player_death_reason;
        net_health_lock.lock();
        if (net_health_pending) {
            hp_value = net_health_value;
            hp_damage = net_health_damage;
            hp_fall_blocks = net_health_fall_blocks;
            hp_hunger = net_health_hunger;
            hp_flags = net_health_flags;
            hp_death_reason = net_health_death_reason;
            net_health_pending = false;
            has_health = true;
        }
        net_health_lock.unlock();
        if (has_health) {
            player_hp = hp_value;
            player_hunger = hp_hunger;
            player_dead = (hp_flags & ge::net::PLAYER_HEALTH_FLAG_DEAD) != 0u || player_hp <= 0.f;
            player_death_reason = hp_death_reason;
            player_last_fall_damage = hp_damage;
            player_last_fall_blocks = hp_fall_blocks;
            if (player_ecs) player_ecs->health[0].hp = static_cast<io::u16>(io::to_u32(player_hp + 0.5f));
            if ((hp_flags & ge::net::PLAYER_HEALTH_FLAG_RESPAWN_RESET) != 0u) {
                use_fly = false;
                use_noclip = false;
                player_crawl_toggle = false;
                if (!TrySetPlayerCrawlMode(false))
                    player_crawling = false;
                player_grounded = false;
                player_airborne = false;
                player_air_peak_foot_y = PlayerFeetY(camera.position);
                SyncLocalPlayerEcsFromRuntime();
            }
            if (player_dead) {
                chunk_requests_inflight.clear();
                ClearInflightLookup();
                PurgeNetChunkRequestCommands();
            }
            if (screen == ScreenState::InGameDead) {
                screen = ScreenState::InGame;
                setCursorVisible(false);
                frame.first_mouse_sample = true;
            }
        }

        bool had_correction = false;
        float corr_x = 0.f, corr_y = 0.f, corr_z = 0.f;
        net_correction_lock.lock();
        if (net_correction_pending) {
            corr_x = net_correction_x;
            corr_y = net_correction_y;
            corr_z = net_correction_z;
            net_correction_pending = false;
            had_correction = true;
        }
        net_correction_lock.unlock();

        if (had_correction) {
            camera.position = { corr_x, corr_y, corr_z };
            move_velocity = { 0.f, 0.f, 0.f };
            player_grounded = false;
            player_airborne = false;
            player_air_peak_foot_y = PlayerFeetY(camera.position);
            player_last_fall_blocks = 0.f;
            player_last_fall_damage = 0.f;
            frame.first_mouse_sample = true;
        }

        if (screen == ScreenState::InGame && !isCursorVisible() && !chat_open && !player_dead) {
            const bool fly_active = use_fly;
            const bool noclip_active = use_noclip;
            static constexpr float SPRINT_MULT = 2.15f;
            static constexpr float CRAWL_SPEED_SCALE = 0.46f;
            static constexpr float MOVE_ACCEL = 42.f;
            static constexpr float MOVE_DECEL = 30.f;
            static constexpr float GRAVITY = 28.f;
            static constexpr float JUMP_SPEED = 8.5f;
            static constexpr float MAX_FALL_SPEED = 52.f;
            static constexpr float WATER_MOVE_SCALE = 0.55f;
            static constexpr float LIQUID_SPRINT_CHANGE = 0.12f;
            static constexpr io::u8 WATER_DENSITY = ge::voxel::fluid_density(ge::voxel::FluidKind::Water);
            static constexpr io::u8 BLOOD_DENSITY = ge::voxel::fluid_density(ge::voxel::FluidKind::Blood);
            static constexpr io::u8 SLIME_DENSITY = ge::voxel::fluid_density(ge::voxel::FluidKind::Slime);
            static constexpr float LIQUID_SCALE_MIN =
                WATER_MOVE_SCALE + (static_cast<float>(WATER_DENSITY) - static_cast<float>(BLOOD_DENSITY)) * LIQUID_SPRINT_CHANGE;
            static constexpr float LIQUID_SCALE_MAX =
                WATER_MOVE_SCALE + (static_cast<float>(WATER_DENSITY) - static_cast<float>(SLIME_DENSITY)) * LIQUID_SPRINT_CHANGE;
            static_assert(LIQUID_SCALE_MAX <= 1.0f, "Liquid speed cannot exceed air speed");
            static_assert(LIQUID_SCALE_MIN >= 0.1f, "Liquid speed cannot be lower than air/10");
            const float dt_clamped = clampf(dt, 0.f, 0.05f);

            lm::vec3 flat_forward{ camera.front[0], 0.f, camera.front[2] };
            const float ff_len2 = lm::vec_dot(flat_forward, flat_forward);
            if (ff_len2 > 0.000001f) {
                const float inv = 1.f / lm::sqrtf(ff_len2);
                flat_forward = flat_forward * inv;
            } else {
                flat_forward = { 0.f, 0.f, -1.f };
            }

            lm::vec3 flat_right = lm::vec3_norm(lm::vec3_cross(flat_forward, ge::Camera::world_up));
            lm::vec3 wish_dir{ 0.f, 0.f, 0.f };
            if (IsActionPressed(Action::MoveForward)) wish_dir += flat_forward;
            if (IsActionPressed(Action::MoveBackward)) wish_dir -= flat_forward;
            if (IsActionPressed(Action::MoveRight)) wish_dir += flat_right;
            if (IsActionPressed(Action::MoveLeft)) wish_dir -= flat_right;
            if (fly_active && IsActionPressed(Action::MoveUp)) wish_dir += ge::Camera::world_up;
            if (fly_active && IsActionPressed(Action::MoveDown)) wish_dir -= ge::Camera::world_up;

            const float wish_len2 = lm::vec_dot(wish_dir, wish_dir);
            if (wish_len2 > 0.000001f)
                wish_dir = wish_dir * (1.f / lm::sqrtf(wish_len2));

            static constexpr io::u8 SPRINT_HUNGER_CRITICAL = 26u;
            const bool can_sprint = player_hunger > SPRINT_HUNGER_CRITICAL;
            const bool sneak_pressed = IsActionPressed(Action::Sneak);
            const bool sprint = can_sprint && !sneak_pressed && IsActionPressed(Action::Sprint);
            if ((fly_active || noclip_active) && player_crawling) {
                if (TrySetPlayerCrawlMode(false))
                    player_crawl_toggle = false;
            } else if (!fly_active && !noclip_active) {
                if (player_crawl_toggle != player_crawling) {
                    if (!TrySetPlayerCrawlMode(player_crawl_toggle)) {
                        if (!player_crawl_toggle)
                            player_crawl_toggle = true;
                    }
                }
            }
            float liquid_scale = 1.f;
            if (!fly_active && !noclip_active && chunk_world_ready) {
                const auto sample_kind = [&](float sx, float sy, float sz) noexcept {
                    const io::i32 wx = floor_to_i32(sx);
                    const io::i32 wy = floor_to_i32(sy);
                    const io::i32 wz = floor_to_i32(sz);
                    return ge::voxel::fluid_kind_from_block_id(voxel_world.get_world_block(wx, wy, wz));
                };

                const float feet_y = PlayerFeetY(camera.position);
                ge::voxel::FluidKind kind = ge::voxel::FluidKind::None;
                const ge::voxel::FluidKind k0 = sample_kind(camera.position[0], feet_y + 0.08f, camera.position[2]);
                const ge::voxel::FluidKind k1 = sample_kind(camera.position[0], feet_y + 0.62f, camera.position[2]);
                const ge::voxel::FluidKind k2 = sample_kind(camera.position[0], feet_y + 1.08f, camera.position[2]);
                kind = k0;
                if (ge::voxel::fluid_density(k1) > ge::voxel::fluid_density(kind)) kind = k1;
                if (ge::voxel::fluid_density(k2) > ge::voxel::fluid_density(kind)) kind = k2;
                if (kind != ge::voxel::FluidKind::None) {
                    const float dens_delta =
                        static_cast<float>(WATER_DENSITY) - static_cast<float>(ge::voxel::fluid_density(kind));
                    liquid_scale = WATER_MOVE_SCALE + dens_delta * LIQUID_SPRINT_CHANGE;
                    liquid_scale = clampf(liquid_scale, LIQUID_SCALE_MIN, LIQUID_SCALE_MAX);
                }
            }
            float target_speed = camera.movement_speed * (sprint ? SPRINT_MULT : 1.f) * liquid_scale;
            if (player_crawling)
                target_speed *= CRAWL_SPEED_SCALE;
            bool sneak_edge_guard = false;
            if (fly_active || noclip_active) {
                if (wish_len2 > 0.000001f) {
                    const lm::vec3 target_vel = wish_dir * target_speed;
                    lm::vec3 dv = target_vel - move_velocity;
                    const float dv_len2 = lm::vec_dot(dv, dv);
                    const float max_change = MOVE_ACCEL * dt_clamped;
                    if (dv_len2 <= max_change * max_change) {
                        move_velocity = target_vel;
                    } else if (dv_len2 > 0.000001f) {
                        dv = dv * (max_change / lm::sqrtf(dv_len2));
                        move_velocity += dv;
                    }
                } else {
                    const float speed2 = lm::vec_dot(move_velocity, move_velocity);
                    if (speed2 > 0.000001f) {
                        const float speed = lm::sqrtf(speed2);
                        const float drop = MOVE_DECEL * dt_clamped;
                        const float next_speed = speed > drop ? speed - drop : 0.f;
                        if (next_speed <= 0.f) move_velocity = { 0.f, 0.f, 0.f };
                        else move_velocity = move_velocity * (next_speed / speed);
                    }
                }
                player_grounded = false;
                player_airborne = false;
            } else {
                lm::vec3 horiz_wish = wish_dir;
                horiz_wish[1] = 0.f;
                const float horiz_len2 = lm::vec_dot(horiz_wish, horiz_wish);
                if (horiz_len2 > 0.000001f)
                    horiz_wish = horiz_wish * (1.f / lm::sqrtf(horiz_len2));

                lm::vec3 horiz_vel{ move_velocity[0], 0.f, move_velocity[2] };
                if (horiz_len2 > 0.000001f) {
                    const lm::vec3 target_h = horiz_wish * target_speed;
                    lm::vec3 dv = target_h - horiz_vel;
                    const float dv_len2 = lm::vec_dot(dv, dv);
                    const float max_change = MOVE_ACCEL * dt_clamped;
                    if (dv_len2 <= max_change * max_change) {
                        horiz_vel = target_h;
                    } else if (dv_len2 > 0.000001f) {
                        dv = dv * (max_change / lm::sqrtf(dv_len2));
                        horiz_vel += dv;
                    }
                } else {
                    const float speed2 = lm::vec_dot(horiz_vel, horiz_vel);
                    if (speed2 > 0.000001f) {
                        const float speed = lm::sqrtf(speed2);
                        const float drop = MOVE_DECEL * dt_clamped;
                        const float next_speed = speed > drop ? speed - drop : 0.f;
                        if (next_speed <= 0.f) horiz_vel = { 0.f, 0.f, 0.f };
                        else horiz_vel = horiz_vel * (next_speed / speed);
                    }
                }
                move_velocity[0] = horiz_vel[0];
                move_velocity[2] = horiz_vel[2];

                bool grounded_before = false;
                if (chunk_world_ready)
                    grounded_before = IsPlayerGroundedAt(camera.position);

                if (grounded_before && !player_crawling && IsActionPressed(Action::MoveUp)) {
                    move_velocity[1] = JUMP_SPEED;
                    grounded_before = false;
                }
                if (grounded_before && (sneak_pressed || player_crawling))
                    sneak_edge_guard = true;
                if (!grounded_before) {
                    move_velocity[1] -= GRAVITY * dt_clamped;
                    if (move_velocity[1] < -MAX_FALL_SPEED)
                        move_velocity[1] = -MAX_FALL_SPEED;
                } else if (move_velocity[1] < 0.f) {
                    move_velocity[1] = 0.f;
                }
            }

            lm::vec3 delta = move_velocity * dt_clamped;
            if (noclip_active)
                camera.position += delta;
            else if (chunk_world_ready) {
                if (sneak_edge_guard) {
                    lm::vec3 edge_test = camera.position;
                    edge_test[0] += delta[0];
                    edge_test[2] += delta[2];
                    if (!IsPlayerGroundedAt(edge_test)) {
                        delta[0] = 0.f;
                        delta[2] = 0.f;
                        move_velocity[0] = 0.f;
                        move_velocity[2] = 0.f;
                    }
                }
                (void)ResolvePlayerBodyPenetration();
                MoveCameraWithCollisions(delta);
                (void)ResolvePlayerBodyPenetration();
            } else
                camera.position += delta;

            if (!fly_active && !noclip_active) {
                const bool grounded_after = chunk_world_ready && IsPlayerGroundedAt(camera.position);
                const float feet_y = PlayerFeetY(camera.position);
                if (!grounded_after) {
                    if (!player_airborne) {
                        player_airborne = true;
                        player_air_peak_foot_y = feet_y;
                    } else if (feet_y > player_air_peak_foot_y) {
                        player_air_peak_foot_y = feet_y;
                    }
                } else {
                    player_airborne = false;
                    if (move_velocity[1] < 0.f) move_velocity[1] = 0.f;
                }
                player_grounded = grounded_after;
            }
        } else {
            move_velocity = { 0.f, 0.f, 0.f };
            player_grounded = false;
            player_airborne = false;
        }

        {
            const bool gameplay_input_active =
                (screen == ScreenState::InGame && !isCursorVisible() && !chat_open && !player_dead);
            const bool sneak_pressed =
                gameplay_input_active && !use_fly && !use_noclip && IsActionPressed(Action::Sneak);
            player_sneaking = sneak_pressed && player_grounded && !player_crawling;
            const float sneak_target = player_sneaking ? 0.28f : 0.f;
            const float sneak_alpha = clampf(dt * 8.f, 0.f, 1.f);
            player_sneak_view_offset += (sneak_target - player_sneak_view_offset) * sneak_alpha;
        }
        player_crawl_prev_grounded = player_grounded;

        UpdateRemotePlayers(dt);
        io::u8 action_flags = 0u;
        if (screen == ScreenState::InGame && !isCursorVisible() && !chat_open && !player_dead &&
            !use_fly && !use_noclip && (IsActionPressed(Action::Sneak) || player_crawling))
            action_flags |= ge::net::PLAYER_ACTION_FLAG_SNEAK;
        if (screen == ScreenState::InGame && !isCursorVisible() && !chat_open && !player_dead &&
            !use_fly && !use_noclip && player_crawling)
            action_flags |= ge::net::PLAYER_ACTION_FLAG_CRAWL;
        if (screen == ScreenState::InGame && !isCursorVisible() && !chat_open && !inventory_open && !player_dead &&
            hi::Key_t::isPressed(hi::Key::MouseRight)) {
            const ge::item::Stack& held_stack = SelectedHotbarStack();
            if (!ge::item::is_empty(held_stack) && ge::item::is_consumable(held_stack.id))
                action_flags |= ge::net::PLAYER_ACTION_FLAG_EAT;
        }

        net_pos_lock.lock();
        net_pos_x = camera.position[0];
        net_pos_y = camera.position[1];
        net_pos_z = camera.position[2];
        net_pos_yaw = camera.yaw;
        net_pos_pitch = camera.pitch;
        net_pos_action_flags = action_flags;
        net_pos_lock.unlock();
        SyncLocalPlayerEcsFromRuntime();
    }
