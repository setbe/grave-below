        // Constants, nested types, and owned runtime state.
        static constexpr io::u32 MAX_PENDING = 8192u;
        static constexpr io::u32 RECV_BUF_CAP = 2048u;
        static constexpr io::u32 PART_BUF_CAP = static_cast<io::u32>(sizeof(ge::net::S2C_ChunkPartHeader) + ge::net::CHUNK_PART_BYTES);
        static constexpr io::u32 INVALID_SLOT = 0xFFFFFFFFu;
        static constexpr io::u16 INVALID_PEER = 0xFFFFu;
        static constexpr io::u32 INVALID_STREAM_SLOT = 0xFFFFFFFFu;

        static constexpr io::i32 STREAM_Y_RADIUS_MIN = 3;
        static constexpr io::u32 STREAM_PER_PEER_TICK = 6u;
        static constexpr io::u32 STREAM_PARTS_PER_TICK = 8u;
        static constexpr io::u32 CHUNK_SENDS_PER_TICK = 1u;
        static constexpr io::u32 PLAYER_SYNC_INTERVAL_MS = 250u;
        static constexpr io::u32 PLAYER_REMOTE_POSE_INTERVAL_MS = 80u;
        static constexpr io::u32 PLAYER_MELEE_COOLDOWN_MS = 320u;
        static constexpr io::u32 SERVER_SIM_TICK_MS = 16u; // ~60 TPS
        static constexpr io::u32 SERVER_SIM_MAX_CATCHUP_TICKS = 4u;
        static constexpr float PLAYER_MAX_SPEED_NO_FLY = 10.f;
        static constexpr float PLAYER_MAX_SPEED_FLY = 28.f;
        static constexpr float PLAYER_POS_BASE_MARGIN = 1.50f;
        static constexpr float PLAYER_REMOTE_POSE_RADIUS = 96.f;
        static constexpr io::u64 PLAYER_POS_DT_CAP_MS = 1200u;
        static constexpr float PLAYER_MAX_VERTICAL_UP_NO_FLY = 0.95f;
        static constexpr float PLAYER_MAX_VERTICAL_DOWN_NO_FLY = 3.50f;
        static constexpr float PLAYER_RADIUS = 0.32f;
        static constexpr float PLAYER_HEIGHT = 1.80f;
        static constexpr float PLAYER_EYE_TO_FEET = 1.62f;
        static constexpr float PLAYER_CRAWL_HEIGHT = 1.00f;
        static constexpr float PLAYER_CRAWL_EYE_TO_FEET = 1.00f;
        static constexpr float PLAYER_COLLIDE_EPS = 0.001f;
        static constexpr float PLAYER_GROUND_PROBE = 0.10f;
        static constexpr float PLAYER_MAX_SPEED_CRAWL = 4.60f;
        static constexpr io::u32 PLAYER_CRAWL_TRANSITION_MS = 220u;
        static constexpr io::u16 PLAYER_HP_MAX = 100u;
        static constexpr io::u8 PLAYER_HUNGER_MAX = 255u;
        static constexpr io::u32 PLAYER_RESPAWN_DELAY_MS = 3000u;
        static constexpr io::u32 PLAYER_VITALS_SYNC_INTERVAL_MS = 1000u;
        static constexpr io::u64 PLAYER_HUNGER_IDLE_TICK_MS = 45000u;
        static constexpr io::u64 PLAYER_HUNGER_STARVE_TICK_MS = 2500u;
        static constexpr float PLAYER_HUNGER_MOVE_DISTANCE = 18.f;
        static constexpr io::u32 PLAYER_POISON_TICK_MS = 1000u;
        static constexpr float PLAYER_SPAWN_X = 0.0f;
        static constexpr float PLAYER_SPAWN_Y = 56.0f;
        static constexpr float PLAYER_SPAWN_Z = 3.0f;
        static constexpr float PLAYER_FALL_DAMAGE_START_BLOCKS = 10.f;
        static constexpr float PLAYER_FALL_DAMAGE_PER_BLOCK = 4.f;
        static constexpr io::u32 SERVER_TPS_BROADCAST_INTERVAL_MS = 1000u;
        static constexpr io::u32 SERVER_WORLD_TIME_BROADCAST_INTERVAL_MS = 500u;
        static constexpr float BLOCK_EDIT_REACH = 8.f;
        static constexpr float PLAYER_MELEE_REACH = 3.25f;
        static constexpr float PLAYER_MELEE_DOT_MIN = 0.45f;
        static constexpr float PLAYER_MELEE_KNOCKBACK_XZ = 1.35f;
        static constexpr float PLAYER_MELEE_KNOCKBACK_Y = 0.26f;
        static constexpr io::u16 PLAYER_MELEE_DAMAGE_DEFAULT = 1u;
        static constexpr io::u16 PLAYER_MELEE_DAMAGE_DAGGER = 10u;
        static constexpr float PLAYER_SPELL_REACH = 16.f;
        static constexpr io::u32 PLAYER_SPELL_COOLDOWN_MIN_MS = 80u;
        static constexpr io::u32 PLAYER_SPELL_PROJECTILE_TTL_MS = 2600u;
        static constexpr io::u32 PLAYER_SPELL_BEAM_TTL_MS = 200u;
        static constexpr io::u32 PLAYER_SPELL_MARK_TTL_MS = 180000u;
        static constexpr io::u32 PLAYER_SPELL_MINE_ARM_MS = 350u;
        static constexpr float PLAYER_SPELL_MINE_TRIGGER_RADIUS = 1.45f;
        static constexpr float PLAYER_SPELL_PULL_RADIUS = 8.5f;
        static constexpr float PLAYER_SPELL_SHIELD_RADIUS = 3.0f;
        static constexpr float PLAYER_SPELL_BLINK_STEP = 4.5f;
        static constexpr io::u64 CHUNK_APP_ACK_TIMEOUT_MS = 2500u;
        static constexpr io::u32 SAND_STEP_INTERVAL_MS = 80u;
        static constexpr io::u32 SAND_SCAN_BUDGET = 1024u;
        static constexpr io::u32 SAND_MOVE_BUDGET = 96u;
        static constexpr io::u32 SAND_CHAIN_STEPS = 1u;
        static constexpr io::u32 SAND_FOCUS_CHUNKS_PER_PEER = 4u;
        static constexpr io::u32 SAND_FOCUS_STREAM_WINDOW = 64u;
        static constexpr io::u32 SAND_SAVE_CAP = 1024u;
        static constexpr io::u32 SAND_ACTIVE_CAP = 131072u;
        // ~3x faster than previous 140ms cadence.
        static constexpr io::u32 WATER_STEP_INTERVAL_MS = 47u;
        static constexpr io::u32 WATER_SCAN_BUDGET = 96u;
        static constexpr io::u32 WATER_EDIT_BUDGET = 24u;
        static constexpr io::u32 WATER_ACTIVE_CAP = 262144u;
        static constexpr io::u32 WATER_FOCUS_CHUNKS_PER_PEER = 4u;
        static constexpr io::u32 WATER_FOCUS_STREAM_WINDOW = 64u;
        static constexpr io::u32 WATER_SAVE_CAP = 1536u;
        static constexpr io::u16 WATER_LEVEL_MAX = 8u;
        static constexpr io::u16 WATER_FLOW_LEVEL_MASK = 0x000Fu;
        static constexpr io::u16 WATER_SOURCE_DISABLED_FLAG = 0x8000u;
        static constexpr io::u16 WATER_TRANSFER_DOWN_MAX = 2u;
        static constexpr io::u16 WATER_TRANSFER_DIAG_MAX = 1u;
        static constexpr io::u16 WATER_TRANSFER_SIDE_MAX = 1u;
        static constexpr io::u32 HOT_BLOCK_PACKED_BYTES = ge::voxel::CHUNK_VOLUME / 2u; // 4 bits per block
        static constexpr io::u32 HOT_SIM_MAX_ENTRIES = 1024u;
        static constexpr io::u32 HOT_SIM_SEED_SCAN_BUDGET = 256u;
        static constexpr io::u32 HOT_SIM_STEP_INTERVAL_MS = 64u;
        static constexpr io::u32 BLOCK_EDIT_DEFER_CAP = 16384u;
        static constexpr io::u32 BLOCK_EDIT_DEFER_FLUSH_BUDGET = 96u;
        static constexpr io::u32 BLOCK_EDIT_DEFER_DEDUP_SCAN = 256u;
        static constexpr io::u8 HOT_FLAG_HAS_COLLISION = 1u << 0;
        static constexpr io::u8 HOT_FLAG_HAS_FALL_SIM = 1u << 1;
        static constexpr io::u8 HOT_FLAG_CAN_PLAYER_SWIM = 1u << 2;
        static constexpr io::u8 HOT_FLAG_HAS_SPECIAL = 1u << 3;
        static constexpr io::u16 WORLD_ACTOR_CAP = 512u;
        static constexpr io::u32 WORLD_ACTOR_REBROADCAST_MS = 250u;
        static constexpr float WORLD_MOB_SEE_RADIUS = 12.f;
        static constexpr float WORLD_MOB_SPEED = 2.75f;
        static constexpr float WORLD_MOB_LOS_STEP = 0.45f;
        static constexpr float WORLD_ITEM_GRAVITY = 18.f;
        static constexpr float WORLD_ITEM_TERMINAL_SPEED = 22.f;
        static constexpr float WORLD_ITEM_PICKUP_RADIUS = 1.65f;
        static constexpr float WORLD_ITEM_MERGE_RADIUS = 3.00f;
        static constexpr io::u32 WORLD_ITEM_DESPAWN_MS = 300000u;
        static constexpr io::u32 WORLD_ITEMS_PER_CHUNK_CAP = 1024u;
        // Hard world-cache clamp to keep RAM bounded even if peer count grows.
        // Simulation may slow down under pressure, but memory usage stays capped.
        static constexpr io::u32 WORLD_CACHE_HARD_CAP = 2048u;
        static constexpr io::u32 TREE_FALL_CAP = 2048u;
        static constexpr io::u32 TREE_SMALL_COMPONENT_CAP = 128u;
        static constexpr io::u32 TREE_FALL_SCAN_CAP = 384u;
        static constexpr io::u32 TREE_GENERATED_BLOCK_CAP = ge::worldgen::SimplexTerrain::TREE_LAYOUT_MAX_BLOCKS;
        static constexpr io::u32 TREE_FALL_STEP_MS = 90u;
        static constexpr io::u32 TREE_FALL_SETTLE_LOG_MS = 900u;
        static constexpr io::u32 TREE_FALL_SETTLE_LEAF_MS = 1700u;
        static constexpr io::u8 TREE_FALL_LEAF_DROP_CHANCE_PERCENT = 35u;
        static_assert(TREE_GENERATED_BLOCK_CAP < 300u, "Generated tree must stay under 300 blocks.");
        static constexpr io::u32 WARD_INSTANCE_CAP = 64u;
        static constexpr io::u32 WARD_SLOT_COUNT = ge::item::INVENTORY_SLOT_COUNT;

        enum SlotState : io::u32 { SlotFree = 0u, SlotQueued = 1u, SlotRunning = 2u, SlotReady = 3u };
        enum StreamState : io::u8 { StreamUnsent = 0u, StreamQueued = 1u, StreamAwaitAck = 2u, StreamSent = 3u };
        enum class WorkerTask : io::u8 { None = 0, BuildChunk = 1 };

        struct PendingReq {
            io::Endpoint to{};
            ge::net::ChunkRequest req{};
            
            io::u32 stream_slot = INVALID_STREAM_SLOT;
            io::u16 peer_index = INVALID_PEER;
            io::u64 enqueued_ms{};
            io::u32 priority{};
        };

        struct SandReservedCell {
            io::i32 x = 0;
            io::i32 y = 0;
            io::i32 z = 0;
        };

        struct SandActiveCell {
            io::i32 wx = 0;
            io::i32 wy = 0;
            io::i32 wz = 0;
        };

        struct WaterPendingEdit {
            io::i32 wx = 0;
            io::i32 wy = 0;
            io::i32 wz = 0;
            ge::voxel::BlockId id = ge::voxel::BlockId::Air;
            io::u16 state = 0u;
        };

        struct WaterActiveCell {
            io::i32 wx = 0;
            io::i32 wy = 0;
            io::i32 wz = 0;
        };

        struct HotChunkSim {
            bool used = false;
            ge::voxel::ChunkCoord coord{};
            io::u32 chunk_version = 0u;
            io::u64 last_touch_ms = 0u;
            io::u8 packed_flags[HOT_BLOCK_PACKED_BYTES]{};
            io::u8 sand_queued[(ge::voxel::CHUNK_VOLUME + 7u) / 8u]{};
            io::u8 water_queued[(ge::voxel::CHUNK_VOLUME + 7u) / 8u]{};
        };

        struct DeferredBlockEdit {
            ge::net::BlockEdit edit{};
            bool prefer_unreliable = false;
        };

        struct TreeScanCell {
            io::i32 x = 0;
            io::i32 y = 0;
            io::i32 z = 0;
        };

        struct TreeFallNode {
            bool active = false;
            ge::voxel::BlockId id = ge::voxel::BlockId::Air;
            io::u16 state = 0u;
            io::i32 x = 0;
            io::i32 y = 0;
            io::i32 z = 0;
            io::u64 next_step_ms = 0u;
            io::u64 settle_until_ms = 0u;
            io::u32 seed = 0u;
            io::u8 phase = 0u; // 0: falling, 1: waiting-to-break
        };

        struct WardInstance {
            bool active = false;
            io::u16 token = 0u;
            io::u8 slots_available = 0u;
            io::u16 stat_speed_x100 = 0u;
            io::u16 stat_delay_cast_x1000 = 0u;
            io::u16 stat_delay_reload_x1000 = 0u;
            io::u16 stat_spread_x100 = 0u;
            ge::item::Stack spells[WARD_SLOT_COUNT]{};
        };

        using ActorEcs = ge::ecs::ActorEcs<WORLD_ACTOR_CAP>;
        using PlayerEcs = ge::ecs::PlayerEcs<static_cast<io::u32>(io::MAX_PEERS)>;

        struct WorkerSlot {
            io::atomic<io::u32> state{ SlotFree };
            io::atomic<io::u32> cancel{ 0u };
            WorkerTask task = WorkerTask::None;
            PendingReq job{};
            ge::voxel::ChunkData chunk{};
            io::u32 hash{};
            io::u8 send_encoding = ge::net::CHUNK_WIRE_ENCODING_RAW;
            io::u16 send_part_count = 0u;
            io::u16 send_part_size = 0u;
            io::u16 send_part_cursor = 0u;
            io::u32 send_total_bytes = 0u;
            bool send_begin_sent = false;
            bool send_end_sent = false;
            bool send_backpressure_latched = false;
            io::vector<io::u8> wire_payload{};
        };

        struct WorkerArg {
            ServerApp* self{};
            io::u32 slot{};
        };

        struct PeerState {
            bool used{};
            io::Endpoint ep{};
            io::u32 session_id{};
            bool has_auth{};
            float auth_x{};
            float auth_y{};
            float auth_z{};
            io::i32 move_dir_x{};
            io::i32 move_dir_y{};
            io::i32 move_dir_z{};
            io::u16 hp = PLAYER_HP_MAX;
            io::u8 hunger = PLAYER_HUNGER_MAX;
            io::u8 last_sent_hunger = PLAYER_HUNGER_MAX;
            io::u16 last_sent_hp = PLAYER_HP_MAX;
            ge::item::PlayerInventory inventory{};
            WardInstance ward_instances[WARD_INSTANCE_CAP]{};
            io::u16 ward_next_token = 1u;
            io::u64 last_inventory_decay_ms{};
            io::u64 next_poison_tick_ms{};
            io::u64 poison_until_ms{};
            io::u64 last_inventory_sync_ms{};
            io::u32 inventory_state_hash{};
            io::u32 last_sent_inventory_hash{};
            bool grounded{};
            bool airborne{};
            io::u32 airborne_ms{};
            float air_peak_foot_y{};
            float hunger_move_accum{};
            bool dead{};
            ge::net::DeathReason death_reason = ge::net::DeathReason::None;
            io::u64 respawn_at_ms{};
            io::u64 next_hunger_tick_ms{};
            io::u64 next_starve_tick_ms{};
            io::u64 next_vitals_sync_ms{};
            bool use_fly{};
            bool use_noclip{};
            bool has_pending{};
            float pending_x{};
            float pending_y{};
            float pending_z{};
            float look_yaw{};
            float look_pitch{};
            io::u8 action_flags{};
            bool crawling{};
            io::u8 crawl_transition_state{}; // 0: none, 1: down, 2: up
            io::u64 crawl_transition_until_ms{};
            io::u8 anim_state = ge::net::PLAYER_ANIM_STILL;
            io::u64 eat_anim_until_ms{};
            io::u64 pending_ms{};
            io::u64 auth_ms{};
            io::u64 last_sync_ms{};
            io::u64 last_remote_pose_broadcast_ms{};
            io::u64 next_melee_ms{};
            io::u16 ward_cast_token = 0u;
            io::u8 ward_cast_cursor = 0u;
            io::u64 ward_next_cast_ms = 0u;
            io::u64 ward_reload_until_ms = 0u;
            ge::voxel::ChunkCoord stream_center{};
            bool stream_center_valid{};
            io::u32 next_request_id = 1u;
            bool chunk_ack_pending{};
            ge::voxel::ChunkCoord chunk_ack_coord{};
            io::u32 chunk_ack_request_id{};
            io::u64 chunk_ack_sent_ms{};
            io::u8 name_len{};
            char name_utf8[ge::net::CHAT_NAME_MAX + 1]{};
            io::u8 signal_quality_nibble{};
            bool roster_announced = false;
            io::u32 sand_focus_cursor{};
            io::u32 water_focus_cursor{};
        };

        io::Socket udp{};
        io::EventLoop<1200, 4096> loop{};

        io::unique_bytes recv_buf_mem{};
        io::u8* recv_buf = nullptr;
        io::unique_bytes part_buf_mem{};
        io::u8* part_buf = nullptr;
        io::unique_bytes terrain_mem{};
        ge::worldgen::SimplexTerrain* terrain = nullptr;
        io::spin_mutex terrain_lock{};
        ge::voxel::World world{};
        ge::build::BlockBuildProfile block_build_profile{};

        io::unique_bytes pending_mem{};
        PendingReq* pending = nullptr;
        io::u32 pending_head{};
        io::u32 pending_tail{};
        io::u32 pending_count{};

        io::unique_bytes pool_mem{};
        io::ThreadPool* pool = nullptr;
        io::unique_bytes worker_slots_mem{};
        WorkerSlot* worker_slots = nullptr;
        io::unique_bytes worker_args_mem{};
        WorkerArg* worker_args = nullptr;

        io::unique_bytes peers_mem{};
        PeerState* peers = nullptr;
        io::unique_bytes stream_order_mem{};
        io::u32* stream_order = nullptr;
        io::unique_bytes stream_state_mem{};
        io::u8* stream_state = nullptr;
        io::unique_bytes stream_reqid_mem{};
        io::u32* stream_reqid = nullptr;
        io::unique_bytes stream_sent_ms_mem{};
        io::u32* stream_sent_ms = nullptr;
        io::unique_bytes sand_touched_mem{};
        ge::voxel::ChunkCoord* sand_touched = nullptr;
        io::unique_bytes sand_reserved_mem{};
        SandReservedCell* sand_reserved = nullptr;
        io::unique_bytes sand_active_mem{};
        SandActiveCell* sand_active = nullptr;
        io::unique_bytes water_touched_mem{};
        ge::voxel::ChunkCoord* water_touched = nullptr;
        io::unique_bytes water_edits_mem{};
        WaterPendingEdit* water_edits = nullptr;
        io::unique_bytes water_active_mem{};
        WaterActiveCell* water_active = nullptr;
        io::unique_bytes hot_sim_mem{};
        HotChunkSim* hot_sim = nullptr;
        io::u32 hot_sim_cap = 0u;
        io::u32 hot_sim_last_idx = 0u;
        io::unique_bytes deferred_block_edits_mem{};
        DeferredBlockEdit* deferred_block_edits = nullptr;
        io::u32 deferred_block_edit_head = 0u;
        io::u32 deferred_block_edit_tail = 0u;
        io::u32 deferred_block_edit_count = 0u;
        io::unique_bytes world_actor_ecs_mem{};
        ActorEcs* world_actor_ecs = nullptr;
        io::unique_bytes player_ecs_mem{};
        PlayerEcs* player_ecs = nullptr;
        TreeFallNode tree_fall_nodes[TREE_FALL_CAP]{};
        io::u32 tree_fall_cursor = 0u;
        TreeScanCell tree_scan_cells[TREE_FALL_SCAN_CAP]{};
        ge::worldgen::SimplexTerrain::TreeBlock tree_generated_blocks[TREE_GENERATED_BLOCK_CAP]{};

        struct WorkerRuntime {
            io::u32 threads{};
            io::u32 slot_count{};
            io::u32 next_free_scan{};
            io::u32 next_ready_scan{};
        } worker{};

        struct StreamRuntime {
            io::u32 distance = 10u;
            io::u32 hot_chunks = 1u;
            io::u32 sx{};
            io::u32 sy{};
            io::u32 sz{};
            io::u32 count{};
            io::i32 y_radius = STREAM_Y_RADIUS_MIN;
        } stream{};

        struct WorldTimeRuntime {
            io::u32 day_ms = 1200000u;   // base day 1 min * 20 by default
            io::u32 night_ms = 900000u;  // base night 0.75 min * 20 by default
            io::u64 epoch_ms = 0u;       // phase 0 anchor in server monotonic clock
            io::u64 next_broadcast_ms = 0u;
        } world_time{};

        struct StatsRuntime {
            io::u32 jobs_submitted{};
            io::u32 jobs_completed{};
            io::u32 jobs_inflight{};
            io::u32 jobs_canceled{};
            io::u32 send_ok{};
            io::u32 send_fail{};
            io::u32 send_backpressure{};
            io::u32 send_backpressure_ticks{};
            io::u32 send_backpressure_packets{};
            io::u32 dropped{};
            io::u32 pos_packets{};
            io::u32 pos_accept{};
            io::u32 pos_reject{};
            io::u32 pos_sync_ok{};
            io::u32 pos_sync_fail{};
            io::u32 chunk_ack_ok{};
            io::u32 chunk_ack_bad{};
            io::u32 chunk_full_sent{};
            io::u32 chunk_empty_sent{};
            io::u32 block_edits_ok{};
            io::u32 block_edits_reject{};
            io::u32 melee_rx{};
            io::u32 melee_hit{};
            io::u32 melee_reject{};
            io::u32 chat_rx{};
            io::u32 chat_cmd{};
            io::u32 chat_tx{};
            io::u32 inventory_rx{};
            io::u32 inventory_tx{};
            io::u32 net_drop_total{};
            io::u32 net_drop_too_small{};
            io::u32 net_drop_bad_magic{};
            io::u32 net_drop_bad_ver{};
            io::u32 net_drop_bad_len{};
            io::u32 net_drop_bad_hs{};
            io::u32 net_drop_bad_ctrl{};
            io::u32 net_drop_bad_mtu{};
            io::u32 net_drop_full_peer_table{};
            io::u64 sand_next_step_ms{};
            io::u32 sand_chunk_cursor{};
            io::u32 sand_linear_cursor{};
            io::u32 sand_active_head{};
            io::u32 sand_active_tail{};
            io::u32 sand_active_count{};
            io::u64 water_next_step_ms{};
            io::u32 water_active_head{};
            io::u32 water_active_tail{};
            io::u32 water_active_count{};
            io::u32 water_seed_chunk_cursor{};
            io::u32 water_seed_linear_cursor{};
            io::u64 next_stats_ms{};
            io::u32 idle_backoff_ms{};
            io::u32 sim_next_ms{};
            io::u32 tps_window_start_ms{};
            io::u32 tps_tick_count{};
        } stats{};

        io::u64 boot_ms{};

