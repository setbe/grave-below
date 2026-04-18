        // Tick scheduling, hot-cache maintenance, telemetry, and UDP callbacks.
        inline void update_peers(io::u64 now_ms) noexcept {
            if (!peers) return;
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                PeerState& p = peers[i];
                if (!p.used) continue;
                age_player_inventory(p, now_ms);
                sync_inventory_if_needed(i, now_ms, false);
                if (p.dead) {
                    p.has_pending = false;
                    sync_player_ecs_from_peer(i);
                    continue;
                }
                if (p.poison_until_ms != 0u) {
                    if (now_ms >= p.poison_until_ms) {
                        p.poison_until_ms = 0u;
                        p.next_poison_tick_ms = 0u;
                    } else if (p.next_poison_tick_ms != 0u && now_ms >= p.next_poison_tick_ms) {
                        if (p.hp > 2u) p.hp = static_cast<io::u16>(p.hp - 2u);
                        else p.hp = 1u;
                        p.next_poison_tick_ms = now_ms + PLAYER_POISON_TICK_MS;
                    }
                }
                if (p.eat_anim_until_ms != 0u && now_ms >= p.eat_anim_until_ms) {
                    p.eat_anim_until_ms = 0u;
                    p.action_flags = static_cast<io::u8>(p.action_flags & static_cast<io::u8>(~ge::net::PLAYER_ACTION_FLAG_EAT));
                }
                if (p.has_pending) {
                    io::u64 pending_ms = p.pending_ms;
                    if (pending_ms == 0u) pending_ms = now_ms;
                    if (!p.has_auth) {
                        const bool requested_sneak = (p.action_flags & ge::net::PLAYER_ACTION_FLAG_SNEAK) != 0u;
                        p.crawling = ((p.action_flags & ge::net::PLAYER_ACTION_FLAG_CRAWL) != 0u) && requested_sneak;
                        if (!p.crawling)
                            p.action_flags = static_cast<io::u8>(p.action_flags & static_cast<io::u8>(~ge::net::PLAYER_ACTION_FLAG_CRAWL));
                        p.auth_x = p.pending_x;
                        p.auth_y = p.pending_y;
                        p.auth_z = p.pending_z;
                        p.move_dir_x = 0;
                        p.move_dir_y = 0;
                        p.move_dir_z = 0;
                        p.auth_ms = pending_ms;
                        p.has_auth = true;
                        p.stream_center_valid = false;
                        p.dead = false;
                        p.death_reason = ge::net::DeathReason::None;
                        if (p.hunger == 0u) p.hunger = PLAYER_HUNGER_MAX;
                        if (p.next_hunger_tick_ms == 0u) p.next_hunger_tick_ms = now_ms + PLAYER_HUNGER_IDLE_TICK_MS;
                        p.next_starve_tick_ms = 0u;
                        if (p.next_vitals_sync_ms == 0u) p.next_vitals_sync_ms = now_ms + PLAYER_VITALS_SYNC_INTERVAL_MS;
                        p.grounded = is_player_grounded(p.auth_x, p.auth_y, p.auth_z, p.crawling);
                        p.airborne = !p.grounded;
                        p.airborne_ms = 0u;
                        p.air_peak_foot_y = p.auth_y - player_eye_to_feet_for_mode(p.crawling);
                        if (p.airborne) p.air_peak_foot_y = p.auth_y - player_eye_to_feet_for_mode(p.crawling);
                        ++stats.pos_accept;
                    } else {
                        io::u64 dt_ms = 16u;
                        if (pending_ms > p.auth_ms)
                            dt_ms = pending_ms - p.auth_ms;
                        if (dt_ms > PLAYER_POS_DT_CAP_MS) dt_ms = PLAYER_POS_DT_CAP_MS;
                        if (dt_ms < 16u) dt_ms = 16u;
                        const io::u32 dt_ms32 = static_cast<io::u32>(dt_ms);

                        const float dt_sec = static_cast<float>(dt_ms32) * 0.001f;
                        const float dx = p.pending_x - p.auth_x;
                        const float dy = p.pending_y - p.auth_y;
                        const float dz = p.pending_z - p.auth_z;
                        const bool sneak_active = (p.action_flags & ge::net::PLAYER_ACTION_FLAG_SNEAK) != 0u;
                        const bool crawl_requested = (p.action_flags & ge::net::PLAYER_ACTION_FLAG_CRAWL) != 0u;
                        const bool was_grounded = p.grounded;
                        const float d2 = dx * dx + dy * dy + dz * dz;
                        const bool trusted_free_move = p.use_fly || p.use_noclip;
                        bool next_crawling = p.crawling;
                        if (trusted_free_move) next_crawling = false;
                        else if (crawl_requested) next_crawling = p.crawling || sneak_active;
                        else next_crawling = false;
                        if (!next_crawling && p.crawling && !p.use_noclip &&
                            is_player_inside_solid(p.pending_x, p.pending_y, p.pending_z, false))
                            next_crawling = true;
                        float max_speed = trusted_free_move ? PLAYER_MAX_SPEED_FLY : PLAYER_MAX_SPEED_NO_FLY;
                        if (!trusted_free_move && next_crawling && max_speed > PLAYER_MAX_SPEED_CRAWL)
                            max_speed = PLAYER_MAX_SPEED_CRAWL;
                        io::u32 airborne_ms_for_check = p.airborne_ms;
                        if (!trusted_free_move && dy < 0.f && p.airborne)
                            airborne_ms_for_check += dt_ms32;
                        if (!trusted_free_move && dy < 0.f) {
                            float fall_allow = PLAYER_MAX_SPEED_NO_FLY + 28.f * (static_cast<float>(airborne_ms_for_check) * 0.001f);
                            if (fall_allow > 90.f) fall_allow = 90.f;
                            if (fall_allow > max_speed) max_speed = fall_allow;
                        }
                        const float max_dist = PLAYER_POS_BASE_MARGIN + max_speed * dt_sec;
                        const float max_allowed_d2 = max_dist * max_dist;
                        bool reject_peer = false;

                        if (d2 > max_allowed_d2) {
                            reject_peer = true;
                        } else if (!trusted_free_move) {
                            const float tick_scale = static_cast<float>(dt_ms32) * 0.02f;
                            const float max_up = PLAYER_MAX_VERTICAL_UP_NO_FLY * tick_scale + 0.10f;
                            const float max_down = PLAYER_MAX_VERTICAL_DOWN_NO_FLY * tick_scale + 0.25f;
                            float max_down_dyn = max_down;
                            if (dy < 0.f && p.airborne) {
                                float extra = (static_cast<float>(airborne_ms_for_check) * 0.001f) * PLAYER_MAX_VERTICAL_DOWN_NO_FLY;
                                if (extra > 12.f) extra = 12.f;
                                max_down_dyn += extra;
                            }
                            if (dy > max_up || dy < -max_down_dyn)
                                reject_peer = true;
                            if (!reject_peer && next_crawling && dy > 0.30f)
                                reject_peer = true;
                        }

                        if (!reject_peer && !p.use_noclip) {
                            float resolved_x = p.pending_x;
                            float resolved_y = p.pending_y;
                            float resolved_z = p.pending_z;
                            if (!try_resolve_player_inside_solid(p.pending_x, p.pending_y, p.pending_z,
                                                                 resolved_x, resolved_y, resolved_z,
                                                                 next_crawling)) {
                                reject_peer = true;
                            } else {
                                p.pending_x = resolved_x;
                                p.pending_y = resolved_y;
                                p.pending_z = resolved_z;
                            }
                        }

                        if (!reject_peer && !trusted_free_move && (sneak_active || next_crawling) && was_grounded) {
                            const bool next_grounded = is_player_grounded(p.pending_x, p.pending_y, p.pending_z, next_crawling);
                            if (!next_grounded) {
                                p.pending_x = p.auth_x;
                                p.pending_y = p.auth_y;
                                p.pending_z = p.auth_z;
                            }
                        }

                        if (reject_peer) {
                            ++stats.pos_reject;
                            (void)send_pos(i, now_ms, ge::net::PLAYER_POS_FLAG_CORRECTION, io::UdpChan::Reliable);
                        } else {
                            const float applied_dx = p.pending_x - p.auth_x;
                            const float applied_dy = p.pending_y - p.auth_y;
                            const float applied_dz = p.pending_z - p.auth_z;
                            p.move_dir_x = 0;
                            p.move_dir_y = 0;
                            p.move_dir_z = 0;
                            if (applied_dx > 0.01f) p.move_dir_x = 1;
                            else if (applied_dx < -0.01f) p.move_dir_x = -1;
                            if (applied_dy > 0.01f) p.move_dir_y = 1;
                            else if (applied_dy < -0.01f) p.move_dir_y = -1;
                            if (applied_dz > 0.01f) p.move_dir_z = 1;
                            else if (applied_dz < -0.01f) p.move_dir_z = -1;
                            p.auth_x = p.pending_x;
                            p.auth_y = p.pending_y;
                            p.auth_z = p.pending_z;
                            const bool was_crawling = p.crawling;
                            if (next_crawling != was_crawling) {
                                p.crawl_transition_state = next_crawling ? 1u : 2u;
                                p.crawl_transition_until_ms = now_ms + PLAYER_CRAWL_TRANSITION_MS;
                            } else if (p.crawl_transition_state != 0u && now_ms >= p.crawl_transition_until_ms) {
                                p.crawl_transition_state = 0u;
                                p.crawl_transition_until_ms = 0u;
                            }
                            p.crawling = next_crawling;
                            if (p.crawling && p.crawl_transition_state == 0u) {
                                if (applied_dy < -0.45f) {
                                    p.crawl_transition_state = 1u;
                                    p.crawl_transition_until_ms = now_ms + PLAYER_CRAWL_TRANSITION_MS;
                                } else if (applied_dy > 0.45f) {
                                    p.crawl_transition_state = 2u;
                                    p.crawl_transition_until_ms = now_ms + PLAYER_CRAWL_TRANSITION_MS;
                                }
                            }
                            if (p.crawling)
                                p.action_flags |= ge::net::PLAYER_ACTION_FLAG_CRAWL;
                            else
                                p.action_flags = static_cast<io::u8>(p.action_flags & static_cast<io::u8>(~ge::net::PLAYER_ACTION_FLAG_CRAWL));
                            const float planar_d2 = applied_dx * applied_dx + applied_dz * applied_dz;
                            const float run_step = 8.0f * dt_sec;
                            const float walk_step = 0.20f * dt_sec;
                            const float crawl_step = 0.08f * dt_sec;
                            io::u8 anim_state = ge::net::PLAYER_ANIM_STILL;
                            if (p.crawl_transition_state == 1u && now_ms < p.crawl_transition_until_ms)
                                anim_state = ge::net::PLAYER_ANIM_CRAWL_DOWN;
                            else if (p.crawl_transition_state == 2u && now_ms < p.crawl_transition_until_ms)
                                anim_state = ge::net::PLAYER_ANIM_CRAWL_UP;
                            else if (p.crawling)
                                anim_state = (planar_d2 > crawl_step * crawl_step)
                                    ? ge::net::PLAYER_ANIM_CRAWL_MOVE
                                    : ge::net::PLAYER_ANIM_CRAWL_IDLE;
                            else if (now_ms < p.eat_anim_until_ms || (p.action_flags & ge::net::PLAYER_ACTION_FLAG_EAT) != 0u)
                                anim_state = ge::net::PLAYER_ANIM_EAT;
                            else if (planar_d2 > run_step * run_step)
                                anim_state = ge::net::PLAYER_ANIM_RUN;
                            else if (planar_d2 > walk_step * walk_step)
                                anim_state = ge::net::PLAYER_ANIM_WALK;
                            p.anim_state = anim_state;
                            p.auth_ms = pending_ms;
                            update_peer_fall_damage(i, p, dt_ms32, now_ms, applied_dx, applied_dy, applied_dz);
                            ++stats.pos_accept;
                        }
                    }
                    p.has_pending = false;
                }
                if (p.crawl_transition_state != 0u && now_ms >= p.crawl_transition_until_ms) {
                    p.crawl_transition_state = 0u;
                    p.crawl_transition_until_ms = 0u;
                }
                sync_player_ecs_from_peer(i);
                if (p.chunk_ack_pending && now_ms - p.chunk_ack_sent_ms >= CHUNK_APP_ACK_TIMEOUT_MS) {
                    p.chunk_ack_pending = false;
                    p.chunk_ack_request_id = 0u;
                }
                if (p.has_auth && now_ms - p.last_sync_ms >= PLAYER_SYNC_INTERVAL_MS)
                    (void)send_pos(i, now_ms, 0u, io::UdpChan::Unreliable);
                if (p.has_auth && now_ms - p.last_remote_pose_broadcast_ms >= PLAYER_REMOTE_POSE_INTERVAL_MS) {
                    broadcast_remote_pose_from_peer(i, now_ms);
                    p.last_remote_pose_broadcast_ms = now_ms;
                }
                if (p.next_vitals_sync_ms == 0u)
                    p.next_vitals_sync_ms = now_ms + PLAYER_VITALS_SYNC_INTERVAL_MS;
                if (now_ms >= p.next_vitals_sync_ms) {
                    (void)send_health(i, now_ms, 0.f, 0.f, io::UdpChan::Unreliable);
                    p.next_vitals_sync_ms = now_ms + PLAYER_VITALS_SYNC_INTERVAL_MS;
                }
            }
        }

        IO_NODISCARD inline io::u32 pending_limit() const noexcept {
            io::u32 lim = worker.slot_count * 8u;
            if (lim < 128u) lim = 128u;
            if (lim > MAX_PENDING - 32u) lim = MAX_PENDING - 32u;
            return lim;
        }

        inline void schedule_stream(io::u64 now_ms) noexcept {
            if (!peers || !stream_order || !stream_state || stream.count == 0u) return;
            if (pending_count >= pending_limit()) return;
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                PeerState& p = peers[i];
                if (!p.used || !p.has_auth) continue;
                const ge::voxel::ChunkCoord center = player_chunk(p);
                if (!p.stream_center_valid || !ge::voxel::coord_eq(center, p.stream_center)) {
                    cleanup_peer(i);
                    stream_reset(i);
                    p.stream_center = center;
                    p.stream_center_valid = true;
                }
                io::u8* states = stream_ptr(i);
                if (!states) continue;
                io::u32 inflight = count_peer_inflight_streams(i);
                io::u32 peer_budget = stream.sx + stream.sy + stream.sz;
                if (peer_budget < 24u) peer_budget = 24u;
                if (peer_budget > 128u) peer_budget = 128u;
                if (peer_budget > stream.count) peer_budget = stream.count;
                if (inflight >= peer_budget) continue;
                const io::u32 now_rel_ms = static_cast<io::u32>(now_ms - boot_ms);
                io::u32 queued = 0u;
                for (io::u32 order_i = 0; order_i < stream.count; ++order_i) {
                    if (pending_count >= pending_limit()) break;
                    if (inflight >= peer_budget) break;
                    const io::u32 stream_slot = stream_order[order_i];
                    if (states[stream_slot] == StreamAwaitAck) {
                        const io::u32 sent_rel_ms = stream_get_sent_ms(i, stream_slot);
                        if (sent_rel_ms != 0u && now_rel_ms - sent_rel_ms >= static_cast<io::u32>(CHUNK_APP_ACK_TIMEOUT_MS)) {
                            states[stream_slot] = StreamUnsent;
                            stream_set_reqid(i, stream_slot, 0u);
                            stream_set_sent_ms(i, stream_slot, 0u);
                        } else {
                            continue;
                        }
                    }
                    if (states[stream_slot] != StreamUnsent) continue;
                    io::i32 ox = 0, oy = 0, oz = 0;
                    stream_offset(stream_slot, ox, oy, oz);
                    ge::net::ChunkRequest req{};
                    req.request_id = p.next_request_id++;
                    req.cx = center.x + ox;
                    req.cy = center.y + oy;
                    req.cz = center.z + oz;
                    req.lod = 0u;
                    if (!enqueue_req(p.ep, req, i, stream_slot, now_ms)) {
                        if (pending_count >= MAX_PENDING) break;
                        continue;
                    }
                    states[stream_slot] = StreamQueued;
                    stream_set_reqid(i, stream_slot, req.request_id);
                    stream_set_sent_ms(i, stream_slot, 0u);
                    ++queued;
                    ++inflight;
                    if (queued >= STREAM_PER_PEER_TICK) break;
                }
            }
        }

        inline void schedule_workers(io::u32 budget_ms) noexcept {
            if (!pool || !worker_slots || !worker_args || pending_count == 0u) return;
            const io::u64 begin = io::monotonic_ms();
            io::u32 scheduled = 0u;
            for (;;) {
                if (scheduled > 0u && io::monotonic_ms() - begin >= budget_ms) break;
                const io::u32 slot_i = find_slot(SlotFree, worker.next_free_scan);
                if (slot_i == INVALID_SLOT) break;
                if (pending_count == 0u) break;
                PendingReq req{};
                if (!dequeue_best_req(req)) break;
                WorkerSlot& slot = worker_slots[slot_i];

                const ge::voxel::ChunkCoord coord{ req.req.cx, req.req.cy, req.req.cz };
                if (ge::voxel::ChunkData* cached = world.find_chunk(coord)) {
                    slot.task = WorkerTask::BuildChunk;
                    slot.cancel.store(0u, io::memory_order_release);
                    clear_slot_backpressure_latch(slot);
                    slot.job = req;
                    copy_chunk_data(slot.chunk, *cached);
                    slot.chunk.generated = true;
                    slot.chunk.dirty_mesh = false;
                    slot.chunk.dirty_neighbors = false;
                    if (!build_chunk_wire_payload(slot)) {
                        slot.task = WorkerTask::None;
                        slot.cancel.store(1u, io::memory_order_release);
                    }
                    slot.state.store(SlotReady, io::memory_order_release);
                    ++stats.jobs_submitted;
                    ++stats.jobs_inflight;
                    ++scheduled;
                    continue;
                }

                slot.task = WorkerTask::BuildChunk;
                slot.cancel.store(0u, io::memory_order_release);
                clear_slot_backpressure_latch(slot);
                slot.job = req;
                slot.hash = 0u;
                slot.send_part_cursor = 0u;
                slot.send_part_count = 0u;
                slot.send_part_size = 0u;
                slot.send_total_bytes = 0u;
                slot.send_begin_sent = false;
                slot.send_end_sent = false;
                slot.wire_payload.clear();
                slot.state.store(SlotQueued, io::memory_order_release);
                WorkerArg& wa = worker_args[slot_i];
                wa.self = this;
                wa.slot = slot_i;
                if (!pool->submit(&ServerApp::worker_entry, &wa)) {
                    slot.task = WorkerTask::None;
                    slot.state.store(SlotFree, io::memory_order_release);
                    (void)enqueue_req(req.to, req.req, req.peer_index, req.stream_slot, req.enqueued_ms);
                    break;
                }
                ++stats.jobs_submitted;
                ++stats.jobs_inflight;
                ++scheduled;
            }
        }

        inline void flush_ready(io::u64 now_ms, io::u32 budget_ms) noexcept {
            if (!worker_slots || worker.slot_count == 0u) return;
            const io::u64 begin = io::monotonic_ms();
            io::u32 done = 0u;
            io::u32 inspected = 0u;
            for (;;) {
                if (done >= CHUNK_SENDS_PER_TICK) break;
                if (done > 0u && io::monotonic_ms() - begin >= budget_ms) break;
                const io::u32 slot_i = find_slot(SlotReady, worker.next_ready_scan);
                if (slot_i == INVALID_SLOT) break;
                ++inspected;
                WorkerSlot& slot = worker_slots[slot_i];
                if (slot.cancel.load(io::memory_order_acquire) != 0u) {
                    stream_set(slot.job.peer_index, slot.job.stream_slot, StreamUnsent);
                    stream_set_reqid(slot.job.peer_index, slot.job.stream_slot, 0u);
                    stream_set_sent_ms(slot.job.peer_index, slot.job.stream_slot, 0u);
                    slot.cancel.store(0u, io::memory_order_release);
                    clear_slot_backpressure_latch(slot);
                    slot.task = WorkerTask::None;
                    slot.wire_payload.clear();
                    slot.state.store(SlotFree, io::memory_order_release);
                    if (stats.jobs_inflight > 0u) --stats.jobs_inflight;
                    ++stats.jobs_canceled;
                    ++done;
                    inspected = 0u;
                    continue;
                }

                bool stale_peer = false;
                if (!peers || slot.job.peer_index >= static_cast<io::u16>(io::MAX_PEERS)) {
                    stale_peer = true;
                } else {
                    const PeerState& p = peers[slot.job.peer_index];
                    if (!p.used || p.dead || !endpoint_eq(p.ep, slot.job.to))
                        stale_peer = true;
                }
                if (stale_peer) {
                    stream_set(slot.job.peer_index, slot.job.stream_slot, StreamUnsent);
                    stream_set_reqid(slot.job.peer_index, slot.job.stream_slot, 0u);
                    stream_set_sent_ms(slot.job.peer_index, slot.job.stream_slot, 0u);
                    slot.cancel.store(0u, io::memory_order_release);
                    clear_slot_backpressure_latch(slot);
                    slot.task = WorkerTask::None;
                    slot.wire_payload.clear();
                    slot.state.store(SlotFree, io::memory_order_release);
                    if (stats.jobs_inflight > 0u) --stats.jobs_inflight;
                    ++stats.jobs_canceled;
                    ++done;
                    inspected = 0u;
                    continue;
                }

                const io::u8 send_status = send_chunk_step(slot, now_ms);
                if (send_status == 0u) {
                    // Reliable backpressure for one slot must not block all ready slots.
                    note_chunk_backpressure(slot);
                    worker.next_ready_scan = (slot_i + 1u) % worker.slot_count;
                    if (inspected >= worker.slot_count)
                        break;
                    continue;
                }
                if (send_status == 1u) {
                    // Partial progress this tick, continue next ready slot scan from here.
                    clear_slot_backpressure_latch(slot);
                    worker.next_ready_scan = slot_i;
                    ++done;
                    inspected = 0u;
                    continue;
                }

                upsert_world_chunk(slot.chunk);

                stream_set(slot.job.peer_index, slot.job.stream_slot, StreamAwaitAck);
                stream_set_sent_ms(slot.job.peer_index, slot.job.stream_slot, static_cast<io::u32>(now_ms - boot_ms));
                if (peers && slot.job.peer_index < static_cast<io::u16>(io::MAX_PEERS)) {
                    PeerState& p = peers[slot.job.peer_index];
                    if (p.used && endpoint_eq(p.ep, slot.job.to)) {
                        p.chunk_ack_pending = false;
                        p.chunk_ack_request_id = 0u;
                    }
                }
                clear_slot_backpressure_latch(slot);
                ++stats.send_ok;
                if (slot.send_total_bytes == 0u) ++stats.chunk_empty_sent;
                else ++stats.chunk_full_sent;
                ++stats.jobs_completed;
                if (stats.jobs_inflight > 0u) --stats.jobs_inflight;
                slot.task = WorkerTask::None;
                slot.wire_payload.clear();
                slot.state.store(SlotFree, io::memory_order_release);
                ++done;
                inspected = 0u;
            }
        }

        IO_NODISCARD inline io::u32 active_peers() const noexcept {
            if (!peers) return 0u;
            io::u32 n = 0u;
            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i)
                if (peers[i].used && peers[i].has_auth && !peers[i].dead) ++n;
            return n;
        }

        IO_NODISCARD inline io::u32 hot_cache_target_chunks() const noexcept {
            const io::u32 peers_now = active_peers();
            if (peers_now == 0u) return 0u;
            const io::u32 side_xz = stream.hot_chunks * 2u + 1u;
            const io::u32 side_y = static_cast<io::u32>(hot_vertical_radius() * 2 + 1);
            if (side_xz == 0u || side_y == 0u) return 0u;
            if (side_xz > (0xFFFFFFFFu / side_xz)) return 0xFFFFFFFFu;
            const io::u32 plane = side_xz * side_xz;
            if (side_y > (0xFFFFFFFFu / plane)) return 0xFFFFFFFFu;
            const io::u32 per_peer = plane * side_y;
            io::u32 target = 0u;
            if (peers_now > (0xFFFFFFFFu / per_peer)) target = 0xFFFFFFFFu;
            else target = per_peer * peers_now;
            if (target > WORLD_CACHE_HARD_CAP) target = WORLD_CACHE_HARD_CAP;
            return target;
        }

        IO_NODISCARD inline bool is_chunk_protected(const ge::voxel::ChunkCoord& coord) const noexcept {
            return is_chunk_hot_any(coord);
        }

        IO_NODISCARD inline io::u32 hot_score_for_chunk(const ge::voxel::ChunkCoord& coord) const noexcept {
            if (!peers) return 0xFFFFFFFFu;
            io::u32 best = 0xFFFFFFFFu;
            bool seen = false;

            for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                const PeerState& p = peers[i];
                if (!p.used || !p.has_auth) continue;
                seen = true;
                const ge::voxel::ChunkCoord center = player_chunk(p);
                const io::i32 dx = coord.x - center.x;
                const io::i32 dy = coord.y - center.y;
                const io::i32 dz = coord.z - center.z;

                const io::u32 d2 = static_cast<io::u32>(dx * dx + dy * dy + dz * dz);
                io::u32 score = d2 * 1024u;
                if (dy < 0) {
                    const io::u32 bonus = static_cast<io::u32>(-dy) * 96u;
                    score = (score > bonus) ? (score - bonus) : 0u;
                } else {
                    score += static_cast<io::u32>(dy) * 96u;
                }

                const io::i32 ahead = dx * p.move_dir_x + dy * p.move_dir_y + dz * p.move_dir_z;
                if (ahead > 0) {
                    const io::u32 bonus = static_cast<io::u32>(ahead) * 256u;
                    score = (score > bonus) ? (score - bonus) : 0u;
                }

                if (score < best) best = score;
            }

            if (!seen) return 0xFFFFFFFFu;
            return best;
        }

        inline void prune_world_hot_cache() noexcept {
            if (world.chunks.empty()) return;
            const io::u32 target = hot_cache_target_chunks();

            if (target == 0u) {
                if (pending_count == 0u && stats.jobs_inflight == 0u) {
                    for (io::usize i = 0; i < world.chunks.size(); ++i) {
                        const ge::voxel::ChunkCoord cc = world.chunks[i].coord;
                        save_chunk_item_drops(cc);
                        despawn_item_actors_in_chunk(cc);
                    }
                    world.clear();
                }
                if (hot_sim && hot_sim_cap > 0u) {
                    for (io::u32 i = 0u; i < hot_sim_cap; ++i)
                        hot_sim[i].used = false;
                }
                return;
            }

            if (world.chunks.size() <= static_cast<io::usize>(target)) return;
            io::u32 to_remove = static_cast<io::u32>(world.chunks.size() - static_cast<io::usize>(target));

            while (to_remove > 0u && !world.chunks.empty()) {
                io::usize victim = static_cast<io::usize>(-1);
                io::u32 victim_score = 0u;
                for (io::usize i = 0; i < world.chunks.size(); ++i) {
                    const ge::voxel::ChunkCoord coord = world.chunks[i].coord;
                    if (is_chunk_protected(coord)) continue;
                    const io::u32 score = hot_score_for_chunk(coord);
                    if (victim == static_cast<io::usize>(-1) || score > victim_score) {
                        victim = i;
                        victim_score = score;
                    }
                }
                if (victim == static_cast<io::usize>(-1))
                    break;

                const ge::voxel::ChunkCoord victim_coord = world.chunks[victim].coord;
                save_chunk_item_drops(victim_coord);
                despawn_item_actors_in_chunk(victim_coord);
                const io::usize last = world.chunks.size() - 1u;
                if (victim != last)
                    world.chunks[victim] = io::move(world.chunks[last]);
                world.chunks.pop_back();
                --to_remove;
            }
            if (hot_sim && hot_sim_cap > 0u) {
                for (io::u32 i = 0u; i < hot_sim_cap; ++i) {
                    if (!hot_sim[i].used) continue;
                    if (!world.find_chunk(hot_sim[i].coord))
                        hot_sim[i].used = false;
                }
            }
        }

        inline void log_stats(io::u64 now_ms) noexcept {
            if (stats.next_stats_ms != 0u && now_ms < stats.next_stats_ms) return;
            const io::u64 interval_ms = active_peers() > 0u ? 1000u : 15000u;
            stats.next_stats_ms = now_ms + interval_ms;
            io::u32 q = 0u, r = 0u, rd = 0u;
            if (worker_slots) {
                for (io::u32 i = 0; i < worker.slot_count; ++i) {
                    const io::u32 s = worker_slots[i].state.load(io::memory_order_acquire);
                    if (s == SlotQueued) ++q;
                    else if (s == SlotRunning) ++r;
                    else if (s == SlotReady) ++rd;
                }
            }
            const io::EventLoop<1200, 4096>::NetDebugSnapshot nd = loop.net_debug_snapshot(now_ms);
            io::out << "[server] stats\n";
            io::out << "  core: peers=" << active_peers()
                << "  q=" << pending_count
                << "  workers=" << worker.threads
                << "  slots(q/r/rd)=" << q << "/" << r << "/" << rd
                << '\n';
            io::out << "  cache: hot(cache/target)=" << world.chunks.size() << "/" << hot_cache_target_chunks()
                << "  jobs(sub/ok/inflight)=" << stats.jobs_submitted << "/" << stats.jobs_completed << "/" << stats.jobs_inflight
                << "  canceled=" << stats.jobs_canceled
                << '\n';
            io::out << "  net: send(ok/fail/backpressure)=" << stats.send_ok << "/" << stats.send_fail << "/" << stats.send_backpressure
                << "  bp(chunk_ticks/packet)=" << stats.send_backpressure_ticks << "/" << stats.send_backpressure_packets
                << "  chunk(full/empty)=" << stats.chunk_full_sent << "/" << stats.chunk_empty_sent
                << "  ack(ok/bad)=" << stats.chunk_ack_ok << "/" << stats.chunk_ack_bad
                << "  edit_defer=" << deferred_block_edit_count
                << '\n';
            io::out << "  gameplay: edit(ok/reject)=" << stats.block_edits_ok << "/" << stats.block_edits_reject
                << "  melee(rx/hit/reject)=" << stats.melee_rx << "/" << stats.melee_hit << "/" << stats.melee_reject
                << "  chat(rx/cmd/tx)=" << stats.chat_rx << "/" << stats.chat_cmd << "/" << stats.chat_tx
                << "  inv(rx/tx)=" << stats.inventory_rx << "/" << stats.inventory_tx
                << '\n';
            io::out << "  motion: pos(pkt/ok/reject)=" << stats.pos_packets << "/" << stats.pos_accept << "/" << stats.pos_reject
                << "  sync(ok/fail)=" << stats.pos_sync_ok << "/" << stats.pos_sync_fail
                << "  drop=" << stats.dropped
                << '\n';
            io::u32 region_active = 0u;
            io::u32 region_dirty = 0u;
            region_count_snapshot(now_ms, region_active, region_dirty);
            io::out << "  region: loaded/active/dirty=" << region_slot_count << "/" << region_active << "/" << region_dirty
                << "  diffuse(touched/skipped)=" << region_stats.diffused << "/" << region_stats.diff_skipped
                << "  sync(ok/fail)=" << region_stats.sync_ok << "/" << region_stats.sync_fail
                << "  save(ok/fail)=" << region_stats.save_ok << "/" << region_stats.save_fail
                << '\n';
            io::out << "  net_debug: sendq(depth/cap/wb)=" << nd.sendq_depth << "/" << nd.sendq_cap << "/" << nd.sendq_wouldblock_streak
                << "  tx(used/cap)=" << nd.tx_arena_used << "/" << nd.tx_arena_cap
                << "  peers(used/cookie/est/rel)=" << nd.peers_used << "/" << nd.peers_cookie_sent << "/" << nd.peers_established << "/" << nd.peers_rel_ready
                << "  rel_inflight=" << nd.rel_inflight_packets
                << '\n';
            io::out << "             poll(call/read/err)=" << nd.poll_calls << "/" << nd.poll_read_ready << "/" << nd.poll_error_ready
                << "  recv(pkt/wblk/creset/other)=" << nd.recv_packets << "/" << nd.recv_wouldblock << "/" << nd.recv_connreset << "/" << nd.recv_other_err
                << "  send_wb(ev/streak/hard/corrupt)=" << nd.send_wouldblock_events << "/" << nd.send_streak_drop << "/" << nd.send_hard_error_drop << "/" << nd.send_corrupt_ref_drop
                << '\n';
            io::out << "             peer_idle(rx/tx/max_ms timeout_hs_overdue)=" << nd.est_max_rx_idle_ms << "/" << nd.est_max_tx_idle_ms
                << "  timeout_candidates=" << nd.est_timeout_candidates
                << "  hs_overdue=" << nd.hs_deadline_overdue
                << "  drop(total/small/magic/ver/len/hs/ctrl/mtu/full)="
                << stats.net_drop_total << "/" << stats.net_drop_too_small << "/" << stats.net_drop_bad_magic << "/"
                << stats.net_drop_bad_ver << "/" << stats.net_drop_bad_len << "/" << stats.net_drop_bad_hs << "/"
                << stats.net_drop_bad_ctrl << "/" << stats.net_drop_bad_mtu << "/" << stats.net_drop_full_peer_table
                << '\n';
        }

        static void on_packet(void* ud, io::Endpoint from, io::u8 type, io::UdpChan chan, io::byte_view payload) noexcept {
            (void)chan;
            ServerApp* app = reinterpret_cast<ServerApp*>(ud);
            if (!app) return;
            if (type == ge::net::PK_C2S_PING) {
                ge::net::C2S_Ping ping{};
                if (!read_payload_exact(payload, ping)) return;
                ge::net::S2C_Pong pong{};
                pong.client_ms_be = ping.client_ms_be;
                pong.server_uptime_ms_be = io::h2nl(static_cast<io::u32>(io::monotonic_ms() - app->boot_ms));
                (void)app->loop.send_to_peer(from, ge::net::PK_S2C_PONG, io::UdpChan::Unreliable, io::byte_view{ reinterpret_cast<const io::u8*>(&pong), sizeof(pong) }, io::monotonic_ms());
                return;
            }
            if (type == ge::net::PK_C2S_PLAYER_POSITION) {
                ge::net::C2S_PlayerPosition wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                const ge::net::PlayerPositionSample sample = ge::net::decode_player_position(wire);
                if (!is_reasonable_world_pos(sample.x, sample.y, sample.z)) {
                    ++app->stats.pos_reject;
                    return;
                }
                PeerState& p = app->peers[peer_index];
                if (p.dead) return;
                p.pending_x = sample.x;
                p.pending_y = sample.y;
                p.pending_z = sample.z;
                p.look_yaw = sample.yaw;
                p.look_pitch = sample.pitch;
                p.action_flags = sample.action_flags;
                p.pending_ms = io::monotonic_ms();
                p.has_pending = true;
                ++app->stats.pos_packets;
                return;
            }
            if (type == ge::net::PK_C2S_MELEE_ATTACK) {
                ge::net::C2S_MeleeAttack wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                ++app->stats.melee_rx;
                app->handle_melee_attack(peer_index, ge::net::decode_c2s_melee_attack(wire), io::monotonic_ms());
                return;
            }
            if (type == ge::net::PK_C2S_CHUNK_ACK) {
                ge::net::C2S_ChunkAck wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                PeerState& p = app->peers[peer_index];
                const ge::net::ChunkAck ack = ge::net::decode_chunk_ack(wire);
                io::u32 stream_slot = INVALID_STREAM_SLOT;
                bool accepted = false;
                if (app->coord_to_stream_slot(p, ack.coord, stream_slot)) {
                    app->stream_set(peer_index, stream_slot, StreamSent);
                    app->stream_set_reqid(peer_index, stream_slot, 0u);
                    app->stream_set_sent_ms(peer_index, stream_slot, 0u);
                    accepted = true;
                } else if (app->find_stream_slot_by_reqid(peer_index, ack.request_id, stream_slot)) {
                    app->stream_set(peer_index, stream_slot, StreamSent);
                    app->stream_set_reqid(peer_index, stream_slot, 0u);
                    app->stream_set_sent_ms(peer_index, stream_slot, 0u);
                    accepted = true;
                } else if (ack.request_id != 0u) {
                    // Client-driven chunk requests may be sent without stream-slot tracking.
                    // Treat such ACK as valid but untracked instead of "bad".
                    accepted = true;
                }
                if (accepted) ++app->stats.chunk_ack_ok;
                else
                    ++app->stats.chunk_ack_bad;
                return;
            }
            if (type == ge::net::PK_C2S_BLOCK_EDIT) {
                ge::net::C2S_BlockEdit wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                PeerState& p = app->peers[peer_index];
                const ge::net::BlockEdit edit = ge::net::decode_c2s_block_edit(wire);
                if (!app->block_edit_allowed(p, edit)) {
                    ++app->stats.block_edits_reject;
                    return;
                }
                const io::u64 now_ms = io::monotonic_ms();
                if (!app->apply_player_block_edit(peer_index, edit, now_ms)) {
                    ++app->stats.block_edits_reject;
                    return;
                }
                ++app->stats.block_edits_ok;
                app->broadcast_block_edit(edit, now_ms);
                return;
            }
            if (type == ge::net::PK_C2S_PLAYER_ROSTER_SELF) {
                ge::net::C2S_PlayerRosterSelf wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                PeerState& p = app->peers[peer_index];
                if (!p.used) return;

                const ge::net::PlayerRosterSelf self_info = ge::net::decode_c2s_player_roster_self(wire);
                p.name_len = self_info.name_len;
                for (io::u32 i = 0u; i < ge::net::CHAT_NAME_MAX; ++i)
                    p.name_utf8[i] = (i < p.name_len) ? self_info.name[i] : '\0';
                p.name_utf8[p.name_len] = '\0';
                p.signal_quality_nibble = ge::net::signal_quality_nibble(self_info.signal_quality);
                p.roster_announced = true;
                app->broadcast_roster_add(peer_index, io::monotonic_ms());
                return;
            }
            if (type == ge::net::PK_C2S_PLAYER_ROSTER_REQUEST) {
                ge::net::C2S_PlayerRosterRequest wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                const ge::net::PlayerRosterRequest req = ge::net::decode_c2s_player_roster_request(wire);
                app->send_roster_window_to_peer(peer_index, req.start_index, req.max_entries, io::monotonic_ms());
                return;
            }
            if (type == ge::net::PK_C2S_CHAT) {
                ge::net::C2S_Chat wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                if (!app->peers[peer_index].used) return;

                const io::u64 now_ms = io::monotonic_ms();
                ge::net::ChatLine line = ge::net::decode_c2s_chat(wire);
                ++app->stats.chat_rx;
                if (line.text_len == 0u) return;
                if (line.name_len > 0u) {
                    io::u8 n = line.name_len;
                    if (n > ge::net::CHAT_NAME_MAX) n = static_cast<io::u8>(ge::net::CHAT_NAME_MAX);
                    app->peers[peer_index].name_len = n;
                    for (io::u32 i = 0; i < ge::net::CHAT_NAME_MAX; ++i)
                        app->peers[peer_index].name_utf8[i] = (i < n) ? line.name[i] : '\0';
                    app->peers[peer_index].name_utf8[n] = '\0';
                }

                io::StackOut<32> fallback_name{};
                io::char_view name{ line.name, line.name_len };
                if (name.empty())
                    name = peer_fallback_name(peer_index, fallback_name);
                const io::char_view text{ line.text, line.text_len };

                if (!text.empty() && text[0] == '/') {
                    ++app->stats.chat_cmd;
                    app->run_chat_command(peer_index, text, now_ms);
                    return;
                }

                ge::net::ChatLine out{};
                fill_chat_line(out, ge::net::CHAT_KIND_PLAYER, name, text);
                app->broadcast_chat(out, now_ms);
                return;
            }
            if (type == ge::net::PK_C2S_REQUEST_CHUNK) {
                ge::net::C2S_RequestChunk wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                PeerState& p = app->peers[peer_index];
                if (!p.used) return;
                if (p.dead) return;
                if (app->pending_count >= app->pending_limit())
                    return;
                ge::net::ChunkRequest req = ge::net::decode_request(wire);
                if (req.request_id == 0u) {
                    req.request_id = p.next_request_id++;
                    if (p.next_request_id == 0u) p.next_request_id = 1u;
                }
                if (!app->request_allowed(p, req)) return;
                (void)app->enqueue_req(p.ep, req, peer_index, INVALID_STREAM_SLOT, io::monotonic_ms());
                return;
            }
            if (type == ge::net::PK_C2S_INVENTORY_ACTION) {
                ge::net::C2S_InventoryAction wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                app->handle_inventory_action(peer_index, ge::net::decode_c2s_inventory_action(wire), io::monotonic_ms());
                return;
            }
            if (type == ge::net::PK_C2S_WARD_CONFIG_ACTION) {
                ge::net::C2S_WardConfigAction wire{};
                if (!read_payload_exact(payload, wire)) return;
                const io::u16 peer_index = app->find_peer(from);
                if (peer_index == INVALID_PEER || !app->peers) return;
                app->handle_ward_config_action(peer_index, ge::net::decode_c2s_ward_config_action(wire), io::monotonic_ms());
                return;
            }
        }

        static void on_tick(void* ud, io::u64 now_ms) noexcept {
            ServerApp* app = reinterpret_cast<ServerApp*>(ud);
            if (!app) return;
            const io::u32 now_rel_ms = static_cast<io::u32>(now_ms - app->boot_ms);
            if (app->stats.sim_next_ms == 0u)
                app->stats.sim_next_ms = now_rel_ms + SERVER_SIM_TICK_MS;

            const io::u32 peers = app->active_peers();
            if (peers == 0u && app->pending_count == 0u && app->stats.jobs_inflight == 0u) {
                app->deferred_block_edit_head = 0u;
                app->deferred_block_edit_tail = 0u;
                app->deferred_block_edit_count = 0u;
                app->prune_world_hot_cache();
                app->region_diffusion_step(now_ms);
                app->region_save_periodic(now_ms);
                io::u32 backoff = app->stats.idle_backoff_ms;
                if (backoff < 2u) backoff = 2u;
                else if (backoff < 64u) backoff <<= 1u;
                if (backoff > 64u) backoff = 64u;
                app->stats.idle_backoff_ms = backoff;
                app->log_stats(now_ms);
                io::sleep_ms(static_cast<int>(backoff));
                return;
            }
            app->stats.idle_backoff_ms = 0u;

            io::u32 sim_ticks = 0u;
            while (now_rel_ms >= app->stats.sim_next_ms && sim_ticks < SERVER_SIM_MAX_CATCHUP_TICKS) {
                const io::u32 worker_budget_ms = (app->pending_count > 0u || app->stats.jobs_inflight > (app->worker.slot_count / 2u)) ? 8u : 4u;
                const io::u32 flush_budget_ms = (app->stats.jobs_inflight > 0u) ? 8u : 2u;
                app->update_peers(now_ms);
                app->update_world_actors(SERVER_SIM_TICK_MS, now_ms);
                app->simulate_sand(now_ms);
                app->simulate_water(now_ms);
                app->update_tree_fall(now_ms);
                app->flush_deferred_block_edits(now_ms);
                app->schedule_workers(worker_budget_ms);
                app->flush_ready(now_ms, flush_budget_ms);
                app->flush_deferred_block_edits(now_ms);
                ++app->stats.tps_tick_count;
                app->stats.sim_next_ms += SERVER_SIM_TICK_MS;
                ++sim_ticks;
            }
            if (sim_ticks == 0u) {
                app->flush_ready(now_ms, 2u);
                app->flush_deferred_block_edits(now_ms);
            }

            app->prune_world_hot_cache();
            app->region_update_active_set(now_ms);
            app->region_diffusion_step(now_ms);
            app->region_sync_peers(now_ms, io::UdpChan::Reliable);
            app->region_save_periodic(now_ms);
            app->sample_and_broadcast_tps(now_ms);
            app->sample_and_broadcast_world_time(now_ms);
            app->log_stats(now_ms);
        }

        static void on_established(void* ud, io::Endpoint peer, io::u32 session_id) noexcept {
            ServerApp* app = reinterpret_cast<ServerApp*>(ud);
            if (!app || !app->peers) return;
            io::u16 idx = app->find_peer(peer);
            if (idx == INVALID_PEER) {
                for (io::u16 i = 0; i < static_cast<io::u16>(io::MAX_PEERS); ++i) {
                    if (app->peers[i].used) continue;
                    app->peers[i] = {};
                    app->peers[i].used = true;
                    app->peers[i].ep = peer;
                    app->peers[i].session_id = session_id;
                    app->peers[i].next_request_id = 1u;
                    app->stream_reset(i);
                    idx = i;
                    break;
                }
            } else {
                app->peers[idx].session_id = session_id;
            }
            if (idx == INVALID_PEER) {
                io::out << "[server] peer table full, dropping peer_port=" << io::n2hs(peer.port_be) << '\n';
                return;
            }
            if (app->player_ecs) app->player_ecs->Activate(idx);
            ServerApp::PeerState& p = app->peers[idx];
            p.hp = ServerApp::PLAYER_HP_MAX;
            p.hunger = ServerApp::PLAYER_HUNGER_MAX;
            p.last_sent_hp = p.hp;
            p.last_sent_hunger = p.hunger;
            p.poison_until_ms = 0u;
            p.next_poison_tick_ms = 0u;
            p.next_hunger_tick_ms = io::monotonic_ms() + ServerApp::PLAYER_HUNGER_IDLE_TICK_MS;
            p.next_starve_tick_ms = 0u;
            p.next_vitals_sync_ms = io::monotonic_ms() + ServerApp::PLAYER_VITALS_SYNC_INTERVAL_MS;
            p.hunger_move_accum = 0.f;
            p.signal_quality_nibble = ge::net::signal_quality_nibble(ge::net::SignalQuality::Bad);
            p.roster_announced = false;
            app->fill_starter_inventory(p);
            p.dead = false;
            p.death_reason = ge::net::DeathReason::None;
            p.respawn_at_ms = 0u;
            app->force_peer_spawn(idx, p, io::monotonic_ms());
            app->sync_player_ecs_from_peer(idx);
            (void)app->send_health(idx, io::monotonic_ms(), 0.f, 0.f, io::UdpChan::Reliable);
            (void)app->send_inventory_to_peer(idx, io::monotonic_ms(), io::UdpChan::Reliable);
            (void)app->send_world_time_to_peer(idx, io::monotonic_ms(), io::UdpChan::Reliable);
            (void)app->region_send_to_peer(idx, io::monotonic_ms(), io::UdpChan::Reliable);
            app->broadcast_world_actor_snapshot_to_peer(idx, io::monotonic_ms());
            if (app->roster_peer_count() <= ge::net::PLAYER_ROSTER_CLIENT_CAP)
                app->send_roster_window_to_peer(idx, 0u, static_cast<io::u16>(ge::net::PLAYER_ROSTER_CLIENT_CAP), io::monotonic_ms());
            io::out << "[server] established session=" << session_id << " peer_port=" << io::n2hs(peer.port_be) << '\n';
        }

        static void on_drop(void* ud, io::Endpoint from, io::Error err, io::DropReason why) noexcept {
            ServerApp* app = reinterpret_cast<ServerApp*>(ud);
            if (app) {
                app->drop_peer_by_endpoint(from);
                ++app->stats.net_drop_total;
                if (why == io::DropReason::TooSmall) ++app->stats.net_drop_too_small;
                else if (why == io::DropReason::BadMagic) ++app->stats.net_drop_bad_magic;
                else if (why == io::DropReason::BadVer) ++app->stats.net_drop_bad_ver;
                else if (why == io::DropReason::BadLen) ++app->stats.net_drop_bad_len;
                else if (why == io::DropReason::BadHs) ++app->stats.net_drop_bad_hs;
                else if (why == io::DropReason::BadCtrl) ++app->stats.net_drop_bad_ctrl;
                else if (why == io::DropReason::BadMtu) ++app->stats.net_drop_bad_mtu;
                else if (why == io::DropReason::FullPeerTable) ++app->stats.net_drop_full_peer_table;
            }
#ifdef _DEBUG
            io::out << "[server] drop from port=" << io::n2hs(from.port_be)
                << " err=" << err << " reason=" << io::drop_reason_str(why) << '\n';
#else
            (void)from; (void)err; (void)why;
#endif
        }

        static void on_disconnect(void* ud, io::Endpoint peer, io::u32 session_id, io::DisconnectReason why) noexcept {
            ServerApp* app = reinterpret_cast<ServerApp*>(ud);
            if (app) app->drop_peer_by_endpoint(peer);
            io::out << "[server] disconnect session=" << session_id
                << " peer_port=" << io::n2hs(peer.port_be)
                << " reason=" << why << '\n';
        }
