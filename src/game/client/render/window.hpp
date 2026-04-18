#pragma once

#include "hi/hi/hi.hpp"
#include "hi/hi/socket.hpp"

#include "../../../engine/core/config.hpp"
#include "../../../engine/core/resource_manager.hpp"
#include "../../../engine/core/modeller.hpp"
#include "../../../engine/core/server_list.hpp"
#include "../../../engine/worldgen/simplex_worldgen.hpp"
#include "../../shared/net/protocol.hpp"
#include "../../shared/world/item.hpp"
#include "../../shared/world/actor_ecs.hpp"
#include "../../shared/world/player_ecs.hpp"
#include "../../shared/world/chunk_stream.hpp"
#include "../../shared/world/block_build_profile.hpp"

#include "client_defs.hpp"
#include "client_ui.hpp"
#include "scene.hpp"
#include "ui.hpp"
#include "../chunk/request.hpp"

struct Window : public hi::Window<Window> {
    hi::AtlasId world_atlas{ -1 };
    hi::ImageAtlasId gui_texture_atlas{ -1 };
    ge::ResourceManager::TextureAtlas texture_atlas{};
    ge::Modeller::BuildResult model_data{};
    io::u32 atlas_tex_gl = 0;
    io::u32 draw_index_count = 0;
    struct Pseudo3dSeamUv {
        bool ready = false;
        bool valid = false;
        bool has_top_span = false;
        bool has_bottom_span = false;
        float left_u = 0.5f;
        float left_v = 0.5f;
        float right_u = 0.5f;
        float right_v = 0.5f;
        float top_u = 0.5f;
        float top_v = 0.5f;
        float top_u0 = 0.5f;
        float top_u1 = 0.5f;
        float bottom_u = 0.5f;
        float bottom_v = 0.5f;
        float bottom_u0 = 0.5f;
        float bottom_u1 = 0.5f;
    };
    Pseudo3dSeamUv pseudo3d_seam_uv_cache[ge::item::ITEM_COUNT]{};

    gl::Buffer vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer ebo{ gl::BufferTarget::ElementArrayBuffer };
    gl::VertexArray vao{};
    gl::Buffer menu_dirt_vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer menu_dirt_ebo{ gl::BufferTarget::ElementArrayBuffer };
    gl::VertexArray menu_dirt_vao{};
    io::u32 menu_dirt_index_count = 0u;
    gl::Buffer menu_stone_vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer menu_stone_ebo{ gl::BufferTarget::ElementArrayBuffer };
    gl::VertexArray menu_stone_vao{};
    io::u32 menu_stone_index_count = 0u;
    gl::Buffer sand_lerp_vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer sand_lerp_ebo{ gl::BufferTarget::ElementArrayBuffer };
    gl::VertexArray sand_lerp_vao{};
    io::u32 sand_lerp_index_count = 0;
    gl::Buffer item_sprite_vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer item_sprite_ebo{ gl::BufferTarget::ElementArrayBuffer };
    gl::VertexArray item_sprite_vao{};
    io::u32 item_sprite_index_count = 0;
    gl::Buffer region_line_vbo{ gl::BufferTarget::ArrayBuffer };
    gl::VertexArray region_line_vao{};
    io::u32 region_line_vertex_count = 0u;
    gl::VertexArray sky_vao{};
    gl::Shader sky_shader{};
    SkyUniforms sky_uniforms{};
    gl::Shader post_effect_shader{};
    PostEffectUniforms post_effect_uniforms{};
    gl::Shader highlight_shader{};
    int highlight_u_mvp = -1;
    int highlight_u_alpha = -1;
    gl::Shader terrain_shader{};
    TerrainUniforms terrain_uniforms{};
    gl::Shader liquid_shader{};
    LiquidUniforms liquid_uniforms{};
    gl::Shader liquid_composite_shader{};
    LiquidCompositeUniforms liquid_composite_uniforms{};
    io::u32 liquid_scene_fbo = 0u;
    io::u32 liquid_scene_color_tex = 0u;
    io::u32 liquid_depth_rbo = 0u;
    io::u32 liquid_accum_fbo = 0u;
    io::u32 liquid_accum_tex = 0u;
    io::u32 liquid_reveal_fbo = 0u;
    io::u32 liquid_reveal_tex = 0u;
    io::u32 liquid_oit_w = 0u;
    io::u32 liquid_oit_h = 0u;
    bool liquid_oit_ready = false;
    gl::Buffer entity_vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer entity_ebo{ gl::BufferTarget::ElementArrayBuffer };
    gl::VertexArray entity_vao{};
    io::u32 entity_index_count = 0;
    gl::Buffer player_entity_vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer player_entity_ebo{ gl::BufferTarget::ElementArrayBuffer };
    gl::VertexArray player_entity_vao{};
    io::u32 player_entity_index_count = 0;
    gl::Shader entity_shader{};
    EntityUniforms entity_uniforms{};
    static constexpr io::u32 ENTITY_BONE_CAP = 64u;
    static constexpr io::u32 ENTITY_CLIP_INVALID = 0xFFFFFFFFu;
    lm::mat4 entity_bones[ENTITY_BONE_CAP]{};
    lm::mat4 entity_local_mats[ENTITY_BONE_CAP]{};
    lm::mat4 entity_global_mats[ENTITY_BONE_CAP]{};
    ge::Modeller::Float3 entity_pose_t[ENTITY_BONE_CAP]{};
    ge::Modeller::Float4 entity_pose_r[ENTITY_BONE_CAP]{};
    ge::Modeller::Float3 entity_pose_s[ENTITY_BONE_CAP]{};
    io::u32 entity_bone_count = 0u;
    io::usize entity_model_index = static_cast<io::usize>(-1);
    io::u32 player_entity_bone_count = 0u;
    io::usize player_entity_model_index = static_cast<io::usize>(-1);
    io::u32 player_clip_still = ENTITY_CLIP_INVALID;
    io::u32 player_clip_walk = ENTITY_CLIP_INVALID;
    io::u32 player_clip_run = ENTITY_CLIP_INVALID;
    io::u32 player_clip_eat = ENTITY_CLIP_INVALID;
    io::u32 player_clip_crawl = ENTITY_CLIP_INVALID;
    io::u32 player_clip_crawl_down = ENTITY_CLIP_INVALID;
    io::u32 player_clip_crawl_up = ENTITY_CLIP_INVALID;
    io::i32 player_head_bone_index = -1;
    io::i32 player_torso_bone_index = -1;
    io::i32 player_right_arm_bone_index = -1;
    io::i32 player_left_leg_bone_index = -1;
    io::i32 player_right_leg_bone_index = -1;
    static constexpr io::usize FIXED_MODEL_SLOT_COUNT = static_cast<io::usize>(ge::Modeller::FixedModelSlot::Count);
    io::usize fixed_model_index_cache[FIXED_MODEL_SLOT_COUNT]{
        static_cast<io::usize>(-1), static_cast<io::usize>(-1), static_cast<io::usize>(-1)
    };
    io::u32 entity_anim_clip_index = 0u;
    io::u32 entity_clip_stay = ENTITY_CLIP_INVALID;
    io::u32 entity_clip_levitate = ENTITY_CLIP_INVALID;
    static constexpr io::u32 WORLD_ACTOR_CAP = 256u;
    using ActorEcs = ge::ecs::ActorEcs<WORLD_ACTOR_CAP>;
    using PlayerEcs = ge::ecs::PlayerEcs<1u>;
    ActorEcs world_actor_ecs_storage{};
    ActorEcs* world_actor_ecs = nullptr;
    PlayerEcs player_ecs_storage{};
    PlayerEcs* player_ecs = nullptr;
    ClientFrameState frame{};
    io::u64 world_time_start_ms = 0u;
    bool target_block_valid = false;
    io::i32 target_block_wx = 0;
    io::i32 target_block_wy = 0;
    io::i32 target_block_wz = 0;
    io::u16 target_block_id = 0u;
    ge::build::BlockBuildProfile block_build_profile{};
    float target_break_progress = 0.f;
    io::i32 target_break_wx = 0;
    io::i32 target_break_wy = 0;
    io::i32 target_break_wz = 0;
    io::u64 target_break_last_submit_ms = 0u;
    BlockFaceUv break_crack_uv[5]{};
    bool setting_rebind = false;
    io::usize rebind_action_index = 0;
    int rebind_step = 0;
    ge::KeyBinding pending_rebind{};

    ScreenState screen = ScreenState::MainMenu;
    GameSessionContext session{};
    ge::Camera camera{};
    lm::vec3 move_velocity{ 0.f, 0.f, 0.f };
    float player_hp = 100.f;
    io::u8 player_hunger = 255u;
    bool player_dead = false;
    ge::net::DeathReason player_death_reason = ge::net::DeathReason::None;
    bool player_grounded = false;
    bool player_airborne = false;
    bool player_sneaking = false;
    bool player_crawling = false;
    bool mouse_left_down = false;
    bool player_crawl_toggle = false;
    bool player_crawl_prev_grounded = false;
    float player_sneak_view_offset = 0.f;
    float player_air_peak_foot_y = 0.f;
    float player_last_fall_blocks = 0.f;
    float player_last_fall_damage = 0.f;
    ge::KeyBinding key_bindings[ACTION_COUNT]{};
    ge::KeyBindingsBinary key_bindings_meta{};
    ge::ServerListBinary server_list_meta{};
    io::vector<ge::ServerListEntry> server_list{};
    ge::ConfigBinary runtime_cfg{};
    ge::voxel::World voxel_world{};
    ge::voxel::PerceptionLevel perception_level = ge::voxel::PerceptionLevel::Normal;
    io::vector<ChunkRenderMesh> chunk_meshes{};
    io::vector<TransparentChunkDrawItem> transparent_chunk_draw{};
    struct ChunkSlotLookupEntry {
        ge::voxel::ChunkCoord coord{};
        io::usize slot = io::npos;
        io::u8 state = 0u; // 0: empty, 1: used, 2: tombstone
    };
    io::vector<ChunkSlotLookupEntry> chunk_slot_lookup{};
    io::u32 chunk_slot_lookup_mask = 0u;
    bool chunk_slot_lookup_dirty = true;
    ChunkMeshJobSlot chunk_job_slots_storage[CHUNK_JOB_MAX_SLOTS]{};
    ChunkMeshJobSlot* chunk_job_slots = nullptr;
    ChunkMeshTaskArg chunk_job_tasks_storage[CHUNK_JOB_MAX_SLOTS]{};
    ChunkMeshTaskArg* chunk_job_tasks = nullptr;
    io::u32 chunk_job_slots_active = 0;
    BlockFaceUv block_face_uv_storage[static_cast<io::usize>(ge::voxel::BLOCK_COUNT) * static_cast<io::usize>(FACE_INDEX_COUNT)]{};
    BlockFaceUv* block_face_uv = nullptr;
    hi::UiColor block_map_tint_storage[static_cast<io::usize>(ge::voxel::BLOCK_COUNT)]{};
    hi::UiColor* block_map_tint = nullptr;
    bool block_map_tint_ready = false;
    bool head_overlay_active = false;
    ge::voxel::BlockId head_overlay_block = ge::voxel::BlockId::Air;
    bool chunk_world_ready = false;
    io::u32 chunk_jobs_submitted = 0;
    io::u32 chunk_jobs_completed = 0;
    io::u32 chunk_jobs_failed = 0;
    io::u32 chunk_faces_last = 0;
    io::u32 chunk_vertices_last = 0;
    io::usize chunk_schedule_cursor = 0;
    io::u32 chunk_meshes_visible = 0;
    io::u32 chunk_meshes_culled = 0;
    io::vector<io::u8> chunk_slot_marks{};
    struct ChunkRequestInflight {
        ge::voxel::ChunkCoord coord{};
        io::u64 sent_ms = 0u;
    };
    io::vector<ChunkRequestInflight> chunk_requests_inflight{};
    struct InflightLookupEntry {
        ge::voxel::ChunkCoord coord{};
        io::usize index = io::npos;
        io::u8 state = 0u; // 0: empty, 1: used, 2: tombstone
    };
    io::vector<InflightLookupEntry> inflight_lookup{};
    io::u32 inflight_lookup_mask = 0u;
    bool inflight_lookup_dirty = true;
    io::u32 chunk_request_scan_cursor = 0u;
    io::i32 chunk_request_radius_phase = 0;
    io::usize chunk_request_virtual_count = 0u;
    io::vector<ge::voxel::ChunkCoord> chunk_request_offsets{};
    io::i32 chunk_request_offsets_rx = -1;
    io::i32 chunk_request_offsets_ry = -1;
    io::i32 chunk_request_offsets_rz = -1;
    ge::voxel::ChunkCoord chunk_center_coord{};
    bool chunk_center_valid = false;
    io::u32 chunk_net_received = 0;

    // Threading and chunk build workers.
    io::ThreadPool worker_pool_storage{};
    io::ThreadPool* worker_pool = nullptr;
    io::u32 hw_threads = 0;
    io::u32 mesh_worker_min = 1;
    io::u32 mesh_worker_max = 1;
    io::u32 mesh_worker_pool = 0;
    io::u32 mesh_workers_configured = 0;
    io::u32 mesh_worker_threads = 0;
    float mesh_workers_pending = 4.f;

    // User/profile and settings UI state.
    float render_distance_pending = 4.f;
    io::u32 render_distance_chunks = 4u;
    float extra_radius_pending = 4.f;
    io::u32 extra_radius = 4u;
    char player_name_utf8[33]{ 'P','l','a','y','e','r','\0' };
    io::usize player_name_len = 6;
    io::u32 player_name_hash = 0;
    io::usize server_selected_index = 0;
    char server_name_input[33]{};
    io::usize server_name_input_len = 0;
    char server_ip_input[49]{};
    io::usize server_ip_input_len = 0;
    char server_port_input[8]{ '2', '5', '5', '6', '5', '\0' };
    io::usize server_port_input_len = 5;
    bool dev_hud_visible = false;
    bool player_hud_visible = false;
    bool is_dark_theme = true;
    enum class InventoryUiTab : io::u8 {
        General = 0u,
        Blocks = 1u,
        Spells = 2u
    };
    bool use_fly = false;
    bool use_noclip = false;
    bool inventory_open = false;
    bool inventory_restore_cursor_visible = false;
    InventoryUiTab inventory_tab = InventoryUiTab::General;
    bool inventory_hover_valid = false;
    ge::item::SlotRegion inventory_hover_region = ge::item::SlotRegion::Hotbar;
    io::u8 inventory_hover_index = 0u;
    static constexpr io::u8 WARD_CONFIG_SLOT_MAX = static_cast<io::u8>(ge::item::INVENTORY_SLOT_COUNT);
    static constexpr io::u32 WARD_CONFIG_CACHE_CAP = 64u;
    struct WardConfigState {
        bool valid = false;
        io::u16 ward_instance = 0u;
        io::u8 slots_available = 0u;
        float stat_speed = 0.f;
        float stat_delay_cast = 0.f;
        float stat_delay_reload = 0.f;
        float stat_spread = 0.f;
        io::u64 snapshot_ms = 0u;
        ge::item::Stack spells[WARD_CONFIG_SLOT_MAX]{};
    };
    ge::item::PlayerInventory inventory_state{};
    bool hotbar_hint_initialized = false;
    io::u8 hotbar_hint_slot = 0u;
    ge::item::Id hotbar_hint_item_id = ge::item::Id::None;
    io::u64 hotbar_hint_until_ms = 0u;
    WardConfigState ward_configs[WARD_CONFIG_CACHE_CAP]{};
    bool ward_config_open = false;
    io::u8 ward_config_selected_index = 0u;
    bool ward_config_hover_valid = false;
    io::u8 ward_config_hover_slot = 0u;
    bool inventory_ward_last_click_valid = false;
    ge::item::SlotRegion inventory_ward_last_click_region = ge::item::SlotRegion::Hotbar;
    io::u8 inventory_ward_last_click_index = 0u;
    io::u64 inventory_ward_last_click_ms = 0u;
    io::u8 inventory_flags = 0u;
    io::u64 inventory_last_snapshot_ms = 0u;
    bool chat_open = false;
    bool chat_suppress_return_reopen = false;
    bool chat_restore_cursor_visible = false;
    bool help_window_open = false;
    bool help_window_restore_cursor_visible = false;
    io::u32 help_window_page = 0u;
    char chat_input_utf8[ge::net::CHAT_TEXT_MAX + 1]{};
    io::usize chat_input_len = 0;
    io::u32 chat_suggestion_count = 0u;
    io::u32 chat_suggestion_selected = 0u;
    char chat_suggestion_text[8][64]{};
    char chat_suggestion_desc[8][96]{};
    io::u8 chat_suggestion_text_len[8]{};
    io::u8 chat_suggestion_desc_len[8]{};
    ge::item::Id chat_suggestion_icon_item[8]{};
    io::u8 chat_arg_help_count = 0u;
    char chat_arg_help[4][96]{};
    io::u8 chat_arg_help_len[4]{};
    static constexpr io::u32 CHAT_HISTORY_CAP = 16u;
    char chat_history_utf8[CHAT_HISTORY_CAP][ge::net::CHAT_TEXT_MAX + 1]{};
    io::u8 chat_history_len[CHAT_HISTORY_CAP]{};
    io::u32 chat_history_next = 0u;
    io::u32 chat_history_count = 0u;
    io::i32 chat_history_nav = -1;
    char chat_history_draft_utf8[ge::net::CHAT_TEXT_MAX + 1]{};
    io::usize chat_history_draft_len = 0u;

    struct HudChatLine {
        io::u8 kind = ge::net::CHAT_KIND_PLAYER;
        io::u8 name_len = 0u;
        io::u8 text_len = 0u;
        io::u32 received_ms32 = 0u;
        char name[ge::net::CHAT_NAME_MAX + 1]{};
        char text[ge::net::CHAT_TEXT_MAX + 1]{};
    };
    static constexpr io::u32 CHAT_LOG_CAP = 24u;
    static constexpr io::u32 CHAT_LINE_HOLD_MS = 7000u;
    static constexpr io::u32 CHAT_LINE_FADE_MS = 3000u;
    HudChatLine chat_log_storage[CHAT_LOG_CAP]{};
    HudChatLine* chat_log = nullptr;
    io::spin_mutex chat_log_lock{};
    io::u32 chat_log_head = 0u;
    io::u32 chat_log_count = 0u;
    HudChatLine chat_render_snapshot[CHAT_LOG_CAP]{};

    io::spin_mutex player_roster_lock{};
    char player_roster_names_storage[static_cast<io::usize>(ge::net::PLAYER_ROSTER_CLIENT_CAP) * ge::net::PLAYER_NICK_BYTES]{};
    char* player_roster_names = nullptr;
    io::u8 player_roster_ping_storage[(ge::net::PLAYER_ROSTER_CLIENT_CAP + 1u) / 2u]{};
    io::u8* player_roster_ping = nullptr;
    io::u8 player_roster_name_len[ge::net::PLAYER_ROSTER_CLIENT_CAP]{};
    io::u16 player_roster_server_index[ge::net::PLAYER_ROSTER_CLIENT_CAP]{};
    io::u32 player_roster_head = 0u;
    io::u32 player_roster_count = 0u;

    static constexpr io::u32 REMOTE_PLAYER_CAP = static_cast<io::u32>(io::MAX_PEERS);
    struct RemotePlayerVisual {
        bool active = false;
        io::u16 server_index = 0xFFFFu;
        io::u8 state = ge::net::PLAYER_ANIM_STILL;
        io::u8 action_flags = 0u;
        ge::item::Id held_item = ge::item::Id::None;
        io::u32 last_update_ms = 0u;
        io::u32 prev_update_ms = 0u;
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
        float raw_x = 0.f;
        float raw_y = 0.f;
        float raw_z = 0.f;
        float vel_x = 0.f;
        float vel_y = 0.f;
        float vel_z = 0.f;
        float target_x = 0.f;
        float target_y = 0.f;
        float target_z = 0.f;
        float yaw = 0.f;
        float target_yaw = 0.f;
        float body_yaw = 0.f;
        float target_body_yaw = 0.f;
        float pitch = 0.f;
        float target_pitch = 0.f;
        float anim_time = 0.f;
    };
    io::spin_mutex remote_players_lock{};
    RemotePlayerVisual remote_players_storage[REMOTE_PLAYER_CAP]{};
    RemotePlayerVisual* remote_players = nullptr;

    struct SandSourceEvent {
        bool used = false;
        io::i32 wx = 0;
        io::i32 wy = 0;
        io::i32 wz = 0;
        io::u64 at_ms = 0;
    };

    struct SandLerpVisual {
        bool active = false;
        float src_x = 0.f;
        float src_y = 0.f;
        float src_z = 0.f;
        float dst_x = 0.f;
        float dst_y = 0.f;
        float dst_z = 0.f;
        io::u64 start_ms = 0;
        io::u32 duration_ms = 0;
    };

    static constexpr io::u32 SAND_SOURCE_EVENT_CAP = 512u;
    static constexpr io::u32 SAND_LERP_VISUAL_CAP = 256u;
    SandSourceEvent sand_source_events_storage[SAND_SOURCE_EVENT_CAP]{};
    SandSourceEvent* sand_source_events = nullptr;
    SandLerpVisual sand_lerp_visuals_storage[SAND_LERP_VISUAL_CAP]{};
    SandLerpVisual* sand_lerp_visuals = nullptr;
    io::u32 sand_source_event_cursor = 0u;
    io::u32 sand_lerp_visual_cursor = 0u;

    // Client network command/data rings.
    static constexpr io::u32 NET_CMD_CAP = 1024u;
    static constexpr io::u32 NET_INCOMING_CAP = 64u;

    enum class NetCmdType : io::u8 {
        RequestChunk = 0,
        BlockEdit = 1,
        Chat = 2,
        InventoryAction = 3,
        MeleeAttack = 4,
        WardConfigAction = 5
    };

    enum class NetIncomingType : io::u8 {
        Chunk = 0,
        BlockEdit = 1,
        Chat = 2,
        WorldActor = 3,
        InventoryState = 4,
        WardConfigState = 5,
        RegionState = 6
    };

    struct NetChunkCmd {
        NetCmdType type = NetCmdType::RequestChunk;
        ge::voxel::ChunkCoord coord{};
        io::u8 lod = 0u;
        io::i32 wx = 0;
        io::i32 wy = 0;
        io::i32 wz = 0;
        io::u16 block_id = 0u;
        io::u16 block_state = 0u;
        float melee_yaw = 0.f;
        float melee_pitch = 0.f;
        ge::net::InventoryAction inventory_action{};
        ge::net::WardConfigActionSample ward_config_action{};
        io::u8 chat_name_len = 0u;
        io::u8 chat_text_len = 0u;
        char chat_name[ge::net::CHAT_NAME_MAX + 1]{};
        char chat_text[ge::net::CHAT_TEXT_MAX + 1]{};
    };

    struct NetIncomingChunk {
        NetIncomingType type = NetIncomingType::Chunk;
        ge::voxel::ChunkData chunk{};
        ge::net::BlockEdit edit{};
        ge::net::ChatLine chat{};
        ge::net::WorldActorSample actor{};
        ge::net::InventoryStateSample inventory{};
        ge::net::WardConfigStateSample ward_config{};
        ge::net::RegionStateSample region_state{};
    };

    io::Socket net_udp{};
    io::EventLoop<1200, 4096>* net_loop = nullptr;
    io::Thread net_thread{};
    static constexpr io::u32 NET_RECV_BUF_CAP = 2048u;
    io::u8 net_recv_buf_storage[NET_RECV_BUF_CAP]{};
    io::u8* net_recv_buf = nullptr;

    io::spin_mutex net_cmd_lock{};
    NetChunkCmd net_cmd_ring_storage[NET_CMD_CAP]{};
    NetChunkCmd* net_cmd_ring = nullptr;
    io::u32 net_cmd_head = 0;
    io::u32 net_cmd_tail = 0;
    io::u32 net_cmd_count = 0;

    io::spin_mutex net_incoming_lock{};
    alignas(NetIncomingChunk) io::u8 net_incoming_ring_storage[sizeof(NetIncomingChunk) * NET_INCOMING_CAP]{};
    NetIncomingChunk* net_incoming_ring = nullptr;
    io::u32 net_incoming_head = 0;
    io::u32 net_incoming_tail = 0;
    io::u32 net_incoming_count = 0;

    io::spin_mutex net_target_lock{};
    io::Endpoint net_target_endpoint{};
    io::Endpoint net_connected_endpoint{};
    alignas(ge::net::ChunkAssembly) io::u8 net_chunk_assembly_storage[sizeof(ge::net::ChunkAssembly)]{};
    ge::net::ChunkAssembly* net_chunk_assembly = nullptr;
    io::spin_mutex net_pos_lock{};
    float net_pos_x = 0.f;
    float net_pos_y = 0.f;
    float net_pos_z = 0.f;
    float net_pos_yaw = 0.f;
    float net_pos_pitch = 0.f;
    io::u8 net_pos_action_flags = 0u;
    io::spin_mutex net_correction_lock{};
    bool net_correction_pending = false;
    float net_correction_x = 0.f;
    float net_correction_y = 0.f;
    float net_correction_z = 0.f;
    io::u64 net_boot_ms = 0;
    io::u64 net_next_handshake_ms = 0;
    io::u64 net_last_ping_sent_ms = 0;
    io::u64 net_last_pos_sent_ms = 0;
    io::u64 net_last_melee_sent_ms = 0;
    io::u32 net_next_request_id = 1u;

    io::atomic<io::u32> net_ready{ 0u };
    io::atomic<io::u32> net_connect_wanted{ 0u };
    io::atomic<io::u32> net_state{ 0u }; // 0 idle, 1 connecting, 2 connected, 3 failed
    io::atomic<io::u32> net_session_id{ 0u };
    io::atomic<io::u32> net_ping_ms{ 0u };
    io::atomic<io::u32> net_ping_avg_ms{ 0u };
    io::atomic<io::u32> net_tps_avg_x100{ 0u };
    io::atomic<io::u32> net_last_drop_reason{ 0u };
    io::atomic<io::u32> net_last_disconnect_reason{ 0u };
    io::atomic<io::u32> net_handshake_attempts{ 0u };
    io::atomic<io::u32> net_received_chunks{ 0u };
    io::atomic<io::u32> net_force_handshake{ 0u };
    static constexpr io::u32 NET_TPS_HIST_CAP = 20u;
    io::u16 net_tps_hist[NET_TPS_HIST_CAP]{};
    io::u32 net_tps_hist_head = 0u;
    io::u32 net_tps_hist_count = 0u;
    io::u32 net_tps_hist_sum = 0u;
    mutable io::spin_mutex net_world_time_lock{};
    bool net_world_time_synced = false;
    io::u32 net_world_phase_ms = 0u;
    io::u32 net_world_day_ms = 1200000u;
    io::u32 net_world_night_ms = 900000u;
    io::u64 net_world_sync_local_ms = 0u;
    io::spin_mutex net_health_lock{};
    bool net_health_pending = false;
    float net_health_value = 100.f;
    float net_health_damage = 0.f;
    float net_health_fall_blocks = 0.f;
    io::u8 net_health_hunger = 255u;
    io::u8 net_health_flags = 0u;
    ge::net::DeathReason net_health_death_reason = ge::net::DeathReason::None;
    io::u64 net_last_roster_report_ms = 0u;
    io::u8 net_last_roster_quality = 0xFFu;

    static constexpr io::u32 CLIENT_REGION_CACHE_CAP = 64u;
    struct ClientRegionStateEntry {
        bool used = false;
        ge::region::RegionId id = 0ull;
        io::u16 mana = ge::region::VALUE_MAX;
        io::u16 instability = 0u;
        io::u16 decay = 0u;
        io::u8 bands = 0u;
        io::u8 flags = 0u;
        io::u32 last_update_ms32 = 0u;
    };
    io::spin_mutex region_state_lock{};
    ClientRegionStateEntry region_state_cache[CLIENT_REGION_CACHE_CAP]{};
    io::u32 region_state_count = 0u;
    ge::region::RegionId region_current_id = 0ull;
    io::u16 region_current_mana = ge::region::VALUE_MAX;
    io::u16 region_current_instability = 0u;
    io::u16 region_current_decay = 0u;
    io::u8 region_current_bands = 0u;
    io::u8 region_neighbor_count = 0u;
    io::u8 region_vertical_neighbor_count = 0u;
    io::u32 region_last_sync_ms32 = 0u;

    Window(const ge::ConfigBinary& cfg) noexcept {
        this->setTitle(ge::WINDOW_TITLE);
        this->setElementScale(2.f);
        this->setTargetFps(0);
        this->setCursorVisible(true);
        gl::Viewport(0, 0, width(), height());
        gl::Enable(gl::Capability::DepthTest);

        camera.position = { 0.0f, 56.0f, 3.0f };
        camera.movement_speed = 3.5f;
        world_time_start_ms = io::monotonic_ms();

        runtime_cfg = cfg;
        LoadRuntimeConfig();
        if (!InitMainStorage()) {
            frame.request_quit = true;
            return;
        }
        InitMeshWorkerPool(mesh_workers_configured);
        (void)LoadServerList();
        SyncServerFormFromSelected();
        if (!InitNetStorage() || !InitNetworkClient())
            net_state.store(3u);
    }
    ~Window() noexcept {
        ShutdownNetworkClient();
        ShutdownNetStorage();
        ClearChunkWorld();
        DestroyLiquidOitBuffers();
        ShutdownMainStorage();
        if (atlas_tex_gl) gl::DeleteTextures(1, &atlas_tex_gl);
    }

    IO_NODISCARD inline bool ShouldQuit() const noexcept { return frame.request_quit; }

    inline void onError(hi::Error, hi::AboutError ae) noexcept override {
        setTitle(hi::what(ae));
    }

    inline void onFocusChange(bool value) noexcept override {
        frame.game_window_focused = value;
    }

    inline void onKeyDown(hi::Key k) noexcept override {
        if (k == hi::Key::MouseLeft)
            mouse_left_down = true;
        if (HandleRebindKeyDown(k)) return;

        if ((screen == ScreenState::InGame || screen == ScreenState::InGameDead) && chat_open) {
            if (k == hi::Key::Up) {
                ChatHistoryPrev();
                return;
            }
            if (k == hi::Key::Down) {
                ChatHistoryNext();
                return;
            }
            if (k == hi::Key::Tab) {
                ApplyChatSuggestionOrAutocomplete();
                return;
            }
            return;
        }

        if (screen == ScreenState::InGame && inventory_open) {
            if (k == hi::Key::MouseLeft || k == hi::Key::MouseRight) {
                if (ward_config_hover_valid) {
                    QueueWardConfigClick(ward_config_hover_slot, k == hi::Key::MouseRight);
                    return;
                }
                if (inventory_hover_valid) {
                    io::u8 ward_slot = 0u;
                    const bool hovered_ward_slot =
                        TryResolveWardSlotFromRegion(inventory_hover_region, inventory_hover_index, ward_slot) &&
                        HasWardAtIndex(ward_slot);
                    if (hovered_ward_slot) {
                        ward_config_selected_index = ward_slot;
                    }

                    if (k == hi::Key::MouseLeft && hovered_ward_slot) {
                        const io::u64 now_ms = io::monotonic_ms();
                        static constexpr io::u64 WARD_CONFIG_DBLCLICK_MS = 300u;
                        const bool same_slot = inventory_ward_last_click_valid &&
                                               inventory_ward_last_click_region == inventory_hover_region &&
                                               inventory_ward_last_click_index == inventory_hover_index;
                        const bool in_window = same_slot &&
                                               now_ms >= inventory_ward_last_click_ms &&
                                               (now_ms - inventory_ward_last_click_ms) <= WARD_CONFIG_DBLCLICK_MS;
                        inventory_ward_last_click_valid = true;
                        inventory_ward_last_click_region = inventory_hover_region;
                        inventory_ward_last_click_index = inventory_hover_index;
                        inventory_ward_last_click_ms = now_ms;
                        if (in_window) {
                            ward_config_open = true;
                            inventory_tab = InventoryUiTab::General;
                            return;
                        }
                    }

                    QueueInventoryClick(inventory_hover_region, inventory_hover_index, k == hi::Key::MouseRight);
                    return;
                }
            }
            if (k == hi::Key::Q && inventory_hover_valid) {
                const bool drop_stack = hi::Key_t::isPressed(hi::Key::Shift);
                const bool drop_one = hi::Key_t::isPressed(hi::Key::Control);
                if (drop_stack || drop_one) {
                    QueueInventoryDropSlot(inventory_hover_region, inventory_hover_index, drop_one);
                    return;
                }
            }
        }

        if (screen == ScreenState::InGame && !isCursorVisible()) {
            if (k == hi::Key::MouseLeft && use_fly) {
                BreakTargetedBlock();
                return;
            }
            if (k == hi::Key::MouseLeft) {
                QueueMeleeAttack();
                return;
            }
            if (k == hi::Key::MouseRight) {
                PlaceSelectedBlock();
                return;
            }
        }
    }

    inline void onKeyUp(hi::Key k) noexcept override {
        if (k == hi::Key::MouseLeft)
            mouse_left_down = false;
        if (setting_rebind) return;

        if ((screen == ScreenState::InGame || screen == ScreenState::InGameDead) && chat_open) {
            if (k == hi::Key::Escape) {
                CloseChatInput();
                return;
            }
            if (IsActionTriggeredOnKeyUp(Action::ChatSuggestionPrev, k)) {
                SelectChatSuggestionDelta(-1);
                return;
            }
            if (IsActionTriggeredOnKeyUp(Action::ChatSuggestionNext, k)) {
                SelectChatSuggestionDelta(1);
                return;
            }
            return;
        }

        if ((screen == ScreenState::InGame || screen == ScreenState::InGameDead) &&
            help_window_open && k == hi::Key::Escape) {
            CloseHelpWindow();
            return;
        }

        if (screen == ScreenState::InGame && inventory_open && k == hi::Key::Escape) {
            CloseInventoryWindow();
            return;
        }

        if (screen == ScreenState::InGame && inventory_open &&
            (k == hi::Key::MouseLeft || k == hi::Key::MouseRight)) {
            if (!inventory_hover_valid && !ward_config_hover_valid && InventoryCursorActive())
                QueueInventoryDropCursor(k == hi::Key::MouseRight);
            return;
        }

        if (screen == ScreenState::InGame && k == hi::Key::F1) {
            setCursorVisible(!isCursorVisible());
            frame.first_mouse_sample = true;
            return;
        }
        if (screen == ScreenState::InGame && k == hi::Key::F2) {
            const bool next = !dev_hud_visible;
            dev_hud_visible = next;
            if (next) player_hud_visible = false;
            return;
        }
        if (screen == ScreenState::InGame && k == hi::Key::F10) {
            const bool next = !player_hud_visible;
            player_hud_visible = next;
            if (next) dev_hud_visible = false;
            return;
        }
        if (screen == ScreenState::InGame && k == hi::Key::F3) {
            SetGodModeEnabled(!use_fly, true);
            PushSystemChat(use_fly ? "godmode: enabled" : "godmode: disabled");
            return;
        }
        if (k == hi::Key::F11) {
            setFullscreen(!isFullscreen());
            return;
        }
        if (k == hi::Key::F12) {
            setVSyncEnable(!isVSync());
            frame.dt_history.clear();
            return;
        }

        if ((screen == ScreenState::InGame || screen == ScreenState::InGameDead) && k == hi::Key::Return) {
            if (chat_suppress_return_reopen) {
                chat_suppress_return_reopen = false;
                return;
            }
            OpenChatInput();
            return;
        }

        if ((screen == ScreenState::InGame || screen == ScreenState::InGameDead) && k == hi::Key::Slash) {
            OpenChatInput("/");
            return;
        }

        if (IsActionTriggeredOnKeyUp(Action::BackToMenu, k)) {
            if (screen == ScreenState::InGame || screen == ScreenState::InGameDead) ReturnToMainMenu();
            else if (screen == ScreenState::KeyBindings) screen = ScreenState::Settings;
            else if (screen == ScreenState::Graphics) screen = ScreenState::Settings;
            else if (screen == ScreenState::Multiplayer) screen = ScreenState::MainMenu;
            else if (screen == ScreenState::Connecting) {
                StopConnect();
                if (session.mode == SessionMode::Singleplayer) {
                    session.mode = SessionMode::None;
                    ClearSessionText();
                    screen = ScreenState::MainMenu;
                } else {
                    screen = ScreenState::Multiplayer;
                }
            }
            else if (screen == ScreenState::Settings) screen = ScreenState::MainMenu;
            return;
        }

        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot1, k)) {
            SelectQuickSlot(0u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot2, k)) {
            SelectQuickSlot(1u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot3, k)) {
            SelectQuickSlot(2u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot4, k)) {
            SelectQuickSlot(3u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot5, k)) {
            SelectQuickSlot(4u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot6, k)) {
            SelectQuickSlot(5u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot7, k)) {
            SelectQuickSlot(6u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot8, k)) {
            SelectQuickSlot(7u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::QuickSlot9, k)) {
            SelectQuickSlot(8u);
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::Crawl, k)) {
            if (IsActionPressed(Action::Sneak))
                player_crawl_toggle = !player_crawl_toggle;
            return;
        }
        if (screen == ScreenState::InGame && IsActionTriggeredOnKeyUp(Action::ToggleInventory, k)) {
            ToggleInventoryWindow();
            return;
        }

        if (IsActionTriggeredOnKeyUp(Action::ToggleWireframe, k)) {
            frame.wireframe_mode = !frame.wireframe_mode;
            gl::PolygonMode(gl::Face::FrontAndBack, frame.wireframe_mode ? gl::Polygon::Line : gl::Polygon::Fill);
            return;
        }
    }

    inline void onTextInput(io::char_view utf8) noexcept override {
        hi::Window<Window>::onTextInput(utf8);
    }

    inline void onMouseMove(int x, int y) noexcept override {
        if (screen != ScreenState::InGame || isCursorVisible()) {
            frame.first_mouse_sample = true;
            return;
        }
        if (frame.first_mouse_sample) {
            frame.first_mouse_sample = false;
            return;
        }

        const int cx = width() / 2;
        const int cy = height() / 2;
        const float dx = static_cast<float>(x - cx);
        const float dy = static_cast<float>(cy - y);
        if (dx != 0.f || dy != 0.f)
            camera.process_mouse_movement(dx, dy);
    }

    inline void onRender(float dt) noexcept override {
        ge::client::render::ScenePipeline::RenderFrame(*this, dt);
    }

    inline int LoadResources() noexcept {
        if (!LoadFonts()) return 1;
        if (!LoadShaders()) return 2;
        if (!LoadBlocksAndAtlas()) return 3;
        if (!LoadKeyBindings()) return 4;
        if (!ge::build::ensure_block_build_profile(block_build_profile))
            ge::build::set_default_block_build_profile(block_build_profile);
        return 0;
    }

    inline ScreenState CurrentScreen() const noexcept { return screen; }
    inline void ScenePipelineUpdateCamera(float dt) noexcept { UpdateCamera(dt); }
    inline void ScenePipelineRenderWorld() noexcept { RenderScene(); }
    inline void ScenePipelineUpdateChunks(const lm::vec3& camera_pos) noexcept { UpdateChunkPipeline(camera_pos); }
    inline void ScenePipelinePumpIncomingChunks() noexcept { PumpIncomingChunks(); }
    inline void ScenePipelineQueueMissingChunks() noexcept { QueueMissingChunkRequests(); }
    inline void ScenePipelineRenderUi(float dt) noexcept { RenderGui(dt); }
    inline void UiPipelineFlushText() noexcept { FlushText(); }
    inline void UiRenderMainMenu(float dt) noexcept { RenderMainMenuGui(dt); }
    inline void UiRenderMultiplayer() noexcept { RenderMultiplayerGui(); }
    inline void UiRenderConnecting() noexcept { RenderConnectingGui(); }
    inline void UiRenderSettings() noexcept { RenderSettingsGui(); }
    inline void UiRenderGraphics() noexcept { RenderGraphicsGui(); }
    inline void UiRenderKeyBindings() noexcept { RenderKeyBindingsGui(); }
    inline void UiRenderInGame(float dt) noexcept { RenderInGameHud(dt); }
    inline void UiRenderInGameDead(float dt) noexcept { RenderInGameDead(dt); }
    inline void PlayerBreakTargetedBlock() noexcept { BreakTargetedBlock(); }
    inline void PlayerPlaceSelectedBlock() noexcept { PlaceSelectedBlock(); }
    IO_NODISCARD inline bool PlayerBodyCollidingAt(const lm::vec3& eye_pos) const noexcept { return IsPlayerBodyCollidingAt(eye_pos); }
    IO_NODISCARD inline bool PlayerGroundedAt(const lm::vec3& eye_pos) const noexcept { return IsPlayerGroundedAt(eye_pos); }

private:
private:
    static inline Action ActionByIndex(io::usize index) noexcept {
        return static_cast<Action>(index);
    }

    template<class T>
    static inline bool ReadPayloadExact(io::byte_view payload, T& out) noexcept {
        if (payload.size() != sizeof(T)) return false;
        io::u8* dst = reinterpret_cast<io::u8*>(&out);
        for (io::usize i = 0; i < sizeof(T); ++i)
            dst[i] = payload[i];
        return true;
    }

    static inline io::u32 HashName(const char* data, io::usize size) noexcept {
        io::u32 h = 2166136261u;
        for (io::usize i = 0; i < size; ++i) {
            h ^= static_cast<io::u8>(data[i]);
            h *= 16777619u;
        }
        return h;
    }

    inline io::char_view PlayerNameView() const noexcept {
        return io::char_view{ player_name_utf8, player_name_len };
    }

    inline void ClampPlayerNameLen() noexcept {
        if (player_name_len > 32) player_name_len = 32;
        player_name_utf8[player_name_len] = '\0';
    }

    static inline io::char_view BlockDisplayName(ge::voxel::BlockId id) noexcept {
        switch (id) {
        case ge::voxel::BlockId::Air: return "Air";
        case ge::voxel::BlockId::Grass: return "Grass";
        case ge::voxel::BlockId::Dirt: return "Dirt";
        case ge::voxel::BlockId::Stone: return "Stone";
        case ge::voxel::BlockId::Sand: return "Sand";
        case ge::voxel::BlockId::Water: return "Water";
        case ge::voxel::BlockId::Blood: return "Blood";
        case ge::voxel::BlockId::Slime: return "Slime";
        case ge::voxel::BlockId::Snow: return "Snow";
        case ge::voxel::BlockId::GrassPale: return "Grass Pale";
        case ge::voxel::BlockId::DirtDry: return "Dirt Dry";
        case ge::voxel::BlockId::StoneCracked: return "Stone Cracked";
        case ge::voxel::BlockId::SandAsh: return "Sand Ash";
        case ge::voxel::BlockId::WaterDark: return "Water Dark";
        case ge::voxel::BlockId::BloodDark: return "Blood Dark";
        case ge::voxel::BlockId::SlimeDark: return "Slime Dark";
        case ge::voxel::BlockId::SnowDirty: return "Snow Dirty";
        case ge::voxel::BlockId::LevitatingBookAnchor: return "Levitating Book";
        case ge::voxel::BlockId::Log: return "Log";
        case ge::voxel::BlockId::Leaves: return "Leaves";
        default: return "Unknown";
        }
    }

    inline void SelectQuickSlot(io::u32 slot_index) noexcept {
        if (slot_index >= ge::item::HOTBAR_SLOT_COUNT) return;
        inventory_state.selected_hotbar = static_cast<io::u8>(slot_index);
        ge::net::InventoryAction action{};
        action.action = ge::net::INVENTORY_ACTION_SELECT_HOTBAR;
        action.src_region = ge::item::SlotRegion::Hotbar;
        action.src_index = static_cast<io::u8>(slot_index);
        (void)EnqueueNetInventoryAction(action);
    }

    static constexpr io::u32 HOTBAR_HINT_DURATION_MS = 2400u;

    inline void ResetHotbarHintTracking() noexcept {
        hotbar_hint_initialized = false;
        hotbar_hint_slot = 0u;
        hotbar_hint_item_id = ge::item::Id::None;
        hotbar_hint_until_ms = 0u;
    }

    inline void UpdateHotbarHintTracking() noexcept {
        const io::u8 selected_slot = (inventory_state.selected_hotbar < ge::item::HOTBAR_SLOT_COUNT)
            ? inventory_state.selected_hotbar : 0u;
        const ge::item::Stack& selected_stack = inventory_state.hotbar[selected_slot];
        if (!hotbar_hint_initialized) {
            hotbar_hint_initialized = true;
            hotbar_hint_slot = selected_slot;
            hotbar_hint_item_id = selected_stack.id;
            return;
        }
        if (hotbar_hint_slot != selected_slot || hotbar_hint_item_id != selected_stack.id) {
            hotbar_hint_slot = selected_slot;
            hotbar_hint_item_id = selected_stack.id;
            if (!ge::item::is_empty(selected_stack))
                hotbar_hint_until_ms = io::monotonic_ms() + HOTBAR_HINT_DURATION_MS;
            else
                hotbar_hint_until_ms = 0u;
        }
    }

    IO_NODISCARD inline const ge::item::Stack& SelectedHotbarStack() const noexcept {
        const io::u32 slot = (inventory_state.selected_hotbar < ge::item::HOTBAR_SLOT_COUNT)
            ? inventory_state.selected_hotbar : 0u;
        return inventory_state.hotbar[slot];
    }

    IO_NODISCARD inline ge::voxel::BlockId SelectedQuickSlotBlock() const noexcept {
        const ge::item::Stack& stack = SelectedHotbarStack();
        if (ge::item::is_empty(stack) || !ge::item::is_placeable(stack.id))
            return ge::voxel::BlockId::Air;
        return ge::item::place_block(stack.id);
    }

    IO_NODISCARD inline io::char_view SelectedQuickSlotLabel() const noexcept {
        const ge::item::Stack& stack = SelectedHotbarStack();
        if (ge::item::is_empty(stack))
            return "Empty";
        return ge::item::name(stack.id);
    }

    inline void RefreshGuiTextureAtlas() noexcept {
        if (atlas_tex_gl == 0u || texture_atlas.atlas_width == 0u || texture_atlas.atlas_height == 0u)
            return;
        if (gui_texture_atlas < 0)
            gui_texture_atlas = RegisterImageAtlas(atlas_tex_gl, texture_atlas.atlas_width, texture_atlas.atlas_height);
        else
            SetImageAtlas(gui_texture_atlas, atlas_tex_gl, texture_atlas.atlas_width, texture_atlas.atlas_height);
    }

    IO_NODISCARD inline io::u32 ItemIconTextureId(const ge::item::Stack& stack) const noexcept {
        if (ge::item::is_empty(stack))
            return ge::ResourceManager::INVALID_ID;

        const io::char_view base_name = ge::item::visual_texture_name(ge::item::resolve_visual(stack));
        if (base_name.empty())
            return ge::ResourceManager::INVALID_ID;

        io::StackOut<96> item_name{};
        item_name << base_name << "_item";
        const io::u32 item_id = ge::ResourceManager::texture_id_of(texture_atlas, item_name.view());
        if (item_id != ge::ResourceManager::INVALID_ID)
            return item_id;
        return ge::ResourceManager::texture_id_of(texture_atlas, base_name);
    }

    IO_NODISCARD inline bool ItemIconUv(const ge::item::Stack& stack,
                                        float& out_u0, float& out_v0,
                                        float& out_u1, float& out_v1) const noexcept {
        const auto swap_v = [](float& v0, float& v1) noexcept {
            const float t = v0;
            v0 = v1;
            v1 = t;
        };
        if (ge::item::is_empty(stack))
            return false;

        const ge::item::Visual visual = ge::item::resolve_visual(stack);
        const io::char_view base_name = ge::item::visual_texture_name(visual);
        if (!base_name.empty()) {
            io::StackOut<96> item_name{};
            item_name << base_name << "_item";
            const io::u32 item_id = ge::ResourceManager::texture_id_of(texture_atlas, item_name.view());
            if (item_id != ge::ResourceManager::INVALID_ID) {
                if (!ge::ResourceManager::texture_uv_of(texture_atlas, item_id, out_u0, out_v0, out_u1, out_v1))
                    return false;
                swap_v(out_v0, out_v1);
                return true;
            }
        }

        const ge::voxel::BlockId block_visual = ge::item::block_from_visual(visual);
        if (block_face_uv && block_visual != ge::voxel::BlockId::Air) {
            const BlockFaceUv front = BlockUvRef(ge::voxel::block_index(block_visual), 0u);
            if (front.valid) {
                out_u0 = front.u0;
                out_v0 = front.v0;
                out_u1 = front.u1;
                out_v1 = front.v1;
                swap_v(out_v0, out_v1);
                return true;
            }
        }

        const io::u32 texture_id = ItemIconTextureId(stack);
        if (texture_id == ge::ResourceManager::INVALID_ID)
            return false;
        if (!ge::ResourceManager::texture_uv_of(texture_atlas, texture_id, out_u0, out_v0, out_u1, out_v1))
            return false;
        swap_v(out_v0, out_v1);
        return true;
    }

    IO_NODISCARD static inline io::u32 ClampTexelCoord(io::i32 value, io::u32 limit) noexcept {
        if (value < 0) return 0u;
        const io::u32 u = static_cast<io::u32>(value);
        if (u >= limit) return (limit > 0u) ? (limit - 1u) : 0u;
        return u;
    }

    inline void InvalidatePseudo3dSeamUvCache() noexcept {
        for (io::u32 i = 0u; i < ge::item::ITEM_COUNT; ++i)
            pseudo3d_seam_uv_cache[i] = {};
    }

    IO_NODISCARD inline bool Pseudo3dSeamUvForItem(const ge::item::Stack& stack, Pseudo3dSeamUv& out) noexcept {
        if (ge::item::is_empty(stack))
            return false;
        const io::u32 idx = ge::item::index(stack.id);
        if (idx >= ge::item::ITEM_COUNT)
            return false;

        Pseudo3dSeamUv& cache = pseudo3d_seam_uv_cache[idx];
        if (cache.ready) {
            if (cache.valid) out = cache;
            return cache.valid;
        }

        cache = {};
        cache.ready = true;
        if (!texture_atlas.atlas_pixels || texture_atlas.atlas_width == 0u || texture_atlas.atlas_height == 0u)
            return false;

        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        if (!ItemIconUv(stack, u0, v0, u1, v1))
            return false;
        const float min_u = (u0 < u1) ? u0 : u1;
        const float max_u = (u0 < u1) ? u1 : u0;
        const float min_v = (v0 < v1) ? v0 : v1;
        const float max_v = (v0 < v1) ? v1 : v0;

        const float w = static_cast<float>(texture_atlas.atlas_width);
        const float h = static_cast<float>(texture_atlas.atlas_height);
        io::i32 x0i = static_cast<io::i32>(min_u * w);
        io::i32 y0i = static_cast<io::i32>(min_v * h);
        io::i32 x1i = static_cast<io::i32>(max_u * w) - 1;
        io::i32 y1i = static_cast<io::i32>(max_v * h) - 1;
        if (x1i < x0i || y1i < y0i)
            return false;

        const io::u32 x0 = ClampTexelCoord(x0i, texture_atlas.atlas_width);
        const io::u32 y0 = ClampTexelCoord(y0i, texture_atlas.atlas_height);
        const io::u32 x1 = ClampTexelCoord(x1i, texture_atlas.atlas_width);
        const io::u32 y1 = ClampTexelCoord(y1i, texture_atlas.atlas_height);
        if (x1 < x0 || y1 < y0)
            return false;

        const io::u32 channels = texture_atlas.atlas_channels > 0u ? texture_atlas.atlas_channels : 4u;
        const io::u8 alpha_threshold = 14u;

        io::u32 ox0 = x1;
        io::u32 oy0 = y1;
        io::u32 ox1 = x0;
        io::u32 oy1 = y0;
        bool found = false;
        for (io::u32 y = y0; y <= y1; ++y) {
            for (io::u32 x = x0; x <= x1; ++x) {
                const io::usize px = (static_cast<io::usize>(y) * texture_atlas.atlas_width + x) * channels;
                const io::u8 a = (channels >= 4u) ? texture_atlas.atlas_pixels[px + 3u] : 255u;
                if (a < alpha_threshold) continue;
                if (!found) {
                    ox0 = ox1 = x;
                    oy0 = oy1 = y;
                    found = true;
                } else {
                    if (x < ox0) ox0 = x;
                    if (x > ox1) ox1 = x;
                    if (y < oy0) oy0 = y;
                    if (y > oy1) oy1 = y;
                }
            }
        }
        if (!found)
            return false;

        double left_sum_y = 0.0, right_sum_y = 0.0, top_sum_x = 0.0, bottom_sum_x = 0.0;
        io::u32 left_count = 0u, right_count = 0u, top_count = 0u, bottom_count = 0u;
        io::u32 top_min_x = ox1, top_max_x = ox0;
        io::u32 bottom_min_x = ox1, bottom_max_x = ox0;
        for (io::u32 y = oy0; y <= oy1; ++y) {
            for (io::u32 x = ox0; x <= ox1; ++x) {
                const io::usize px = (static_cast<io::usize>(y) * texture_atlas.atlas_width + x) * channels;
                const io::u8 a = (channels >= 4u) ? texture_atlas.atlas_pixels[px + 3u] : 255u;
                if (a < alpha_threshold) continue;
                if (x == ox0) { left_sum_y += static_cast<double>(y); ++left_count; }
                if (x == ox1) { right_sum_y += static_cast<double>(y); ++right_count; }
                if (y == oy0) {
                    top_sum_x += static_cast<double>(x);
                    ++top_count;
                    if (x < top_min_x) top_min_x = x;
                    if (x > top_max_x) top_max_x = x;
                }
                if (y == oy1) {
                    bottom_sum_x += static_cast<double>(x);
                    ++bottom_count;
                    if (x < bottom_min_x) bottom_min_x = x;
                    if (x > bottom_max_x) bottom_max_x = x;
                }
            }
        }

        const float inv_w = 1.f / w;
        const float inv_h = 1.f / h;
        const auto texel_center_u = [inv_w](double x) noexcept { return static_cast<float>((x + 0.5) * inv_w); };
        const auto texel_center_v = [inv_h](double y) noexcept { return static_cast<float>((y + 0.5) * inv_h); };
        const double mid_x = 0.5 * static_cast<double>(ox0 + ox1);
        const double mid_y = 0.5 * static_cast<double>(oy0 + oy1);

        cache.left_u = texel_center_u(static_cast<double>(ox0));
        cache.right_u = texel_center_u(static_cast<double>(ox1));
        cache.top_v = texel_center_v(static_cast<double>(oy0));
        cache.bottom_v = texel_center_v(static_cast<double>(oy1));
        cache.left_v = texel_center_v(left_count > 0u ? (left_sum_y / static_cast<double>(left_count)) : mid_y);
        cache.right_v = texel_center_v(right_count > 0u ? (right_sum_y / static_cast<double>(right_count)) : mid_y);
        cache.top_u = texel_center_u(top_count > 0u ? (top_sum_x / static_cast<double>(top_count)) : mid_x);
        cache.bottom_u = texel_center_u(bottom_count > 0u ? (bottom_sum_x / static_cast<double>(bottom_count)) : mid_x);
        if (top_count > 0u) {
            cache.has_top_span = true;
            cache.top_u0 = texel_center_u(static_cast<double>(top_min_x));
            cache.top_u1 = texel_center_u(static_cast<double>(top_max_x));
        }
        if (bottom_count > 0u) {
            cache.has_bottom_span = true;
            cache.bottom_u0 = texel_center_u(static_cast<double>(bottom_min_x));
            cache.bottom_u1 = texel_center_u(static_cast<double>(bottom_max_x));
        }
        cache.valid = true;
        out = cache;
        return true;
    }

    IO_NODISCARD inline hi::UiColor AverageTintFromUv(const BlockFaceUv& uv) const noexcept {
        if (!uv.valid || !texture_atlas.atlas_pixels || texture_atlas.atlas_width == 0u || texture_atlas.atlas_height == 0u)
            return ge::ui::Color(0.52f, 0.56f, 0.62f, 0.28f);

        const float w = static_cast<float>(texture_atlas.atlas_width);
        const float h = static_cast<float>(texture_atlas.atlas_height);
        io::i32 x0i = static_cast<io::i32>(uv.u0 * w);
        io::i32 y0i = static_cast<io::i32>(uv.v0 * h);
        io::i32 x1i = static_cast<io::i32>(uv.u1 * w) - 1;
        io::i32 y1i = static_cast<io::i32>(uv.v1 * h) - 1;
        if (x1i < x0i) {
            const io::i32 t = x0i; x0i = x1i; x1i = t;
        }
        if (y1i < y0i) {
            const io::i32 t = y0i; y0i = y1i; y1i = t;
        }
        const io::u32 x0 = ClampTexelCoord(x0i, texture_atlas.atlas_width);
        const io::u32 y0 = ClampTexelCoord(y0i, texture_atlas.atlas_height);
        const io::u32 x1 = ClampTexelCoord(x1i, texture_atlas.atlas_width);
        const io::u32 y1 = ClampTexelCoord(y1i, texture_atlas.atlas_height);
        if (x1 < x0 || y1 < y0)
            return ge::ui::Color(0.52f, 0.56f, 0.62f, 0.28f);

        const io::u32 channels = texture_atlas.atlas_channels >= 3u ? texture_atlas.atlas_channels : 4u;
        double sum_r = 0.0;
        double sum_g = 0.0;
        double sum_b = 0.0;
        double sum_a = 0.0;
        io::u32 sample_count = 0u;
        for (io::u32 y = y0; y <= y1; ++y) {
            for (io::u32 x = x0; x <= x1; ++x) {
                const io::usize px = (static_cast<io::usize>(y) * texture_atlas.atlas_width + x) * channels;
                const io::u8 r = texture_atlas.atlas_pixels[px + 0u];
                const io::u8 g = texture_atlas.atlas_pixels[px + 1u];
                const io::u8 b = texture_atlas.atlas_pixels[px + 2u];
                const io::u8 a = (channels >= 4u) ? texture_atlas.atlas_pixels[px + 3u] : 255u;
                const double af = static_cast<double>(a) / 255.0;
                sum_r += static_cast<double>(r) * af;
                sum_g += static_cast<double>(g) * af;
                sum_b += static_cast<double>(b) * af;
                sum_a += af;
                ++sample_count;
            }
        }
        if (sample_count == 0u || sum_a <= 0.00001)
            return ge::ui::Color(0.52f, 0.56f, 0.62f, 0.28f);

        float out_r = static_cast<float>((sum_r / sum_a) / 255.0);
        float out_g = static_cast<float>((sum_g / sum_a) / 255.0);
        float out_b = static_cast<float>((sum_b / sum_a) / 255.0);
        const float raw_alpha = static_cast<float>(sum_a / static_cast<double>(sample_count));
        float out_a = raw_alpha * 1.8f + 0.12f; // normalized alpha with visibility boost
        if (out_a < 0.12f) out_a = 0.12f;
        if (out_a > 0.68f) out_a = 0.68f;
        return ge::ui::Color(out_r, out_g, out_b, out_a);
    }

    inline void BuildBlockMapTintTable() noexcept {
        if (!block_map_tint || !block_face_uv) return;
        const io::usize block_count = static_cast<io::usize>(ge::voxel::BLOCK_COUNT);
        for (io::usize bid = 0u; bid < block_count; ++bid) {
            hi::UiColor tint = ge::ui::Color(0.52f, 0.56f, 0.62f, 0.28f);
            BlockFaceUv uv = BlockUvRef(static_cast<io::u16>(bid), 0u); // side face by default
            if (!uv.valid) {
                for (io::u8 f = 0u; f < FACE_INDEX_COUNT; ++f) {
                    const BlockFaceUv cand = BlockUvRef(static_cast<io::u16>(bid), f);
                    if (!cand.valid) continue;
                    uv = cand;
                    break;
                }
            }
            if (uv.valid)
                tint = AverageTintFromUv(uv);
            block_map_tint[bid] = tint;
        }
        block_map_tint_ready = true;
    }

    inline void BuildBreakCrackUvTable() noexcept {
        static const io::char_view crack_names[5]{
            "crack1", "crack2", "crack3", "crack4", "crack5"
        };
        for (io::u32 i = 0u; i < 5u; ++i) {
            break_crack_uv[i] = {};
            (void)ResolveTextureUv(crack_names[i], break_crack_uv[i]);
        }
    }

    inline void ClearRegionStateCache() noexcept {
        region_state_lock.lock();
        for (io::u32 i = 0u; i < CLIENT_REGION_CACHE_CAP; ++i)
            region_state_cache[i] = {};
        region_state_count = 0u;
        region_current_id = 0ull;
        region_current_mana = ge::region::VALUE_MAX;
        region_current_instability = 0u;
        region_current_decay = 0u;
        region_current_bands = 0u;
        region_neighbor_count = 0u;
        region_vertical_neighbor_count = 0u;
        region_last_sync_ms32 = 0u;
        region_state_lock.unlock();
    }

    static inline void UpsertRegionStateEntry(io::view<ClientRegionStateEntry> cache,
                                              io::u32& io_count,
                                              const ge::net::RegionEntrySample& entry,
                                              io::u32 now_ms32) noexcept {
        io::u32 free_idx = CLIENT_REGION_CACHE_CAP;
        for (io::u32 i = 0u; i < CLIENT_REGION_CACHE_CAP; ++i) {
            if (!cache[i].used) {
                if (free_idx == CLIENT_REGION_CACHE_CAP) free_idx = i;
                continue;
            }
            if (cache[i].id != entry.region_id)
                continue;
            cache[i].mana = entry.mana;
            cache[i].instability = entry.instability;
            cache[i].decay = entry.decay;
            cache[i].bands = entry.bands;
            cache[i].flags = entry.flags;
            cache[i].last_update_ms32 = now_ms32;
            return;
        }
        io::u32 idx = free_idx;
        if (idx == CLIENT_REGION_CACHE_CAP) {
            idx = 0u;
            io::u32 oldest = cache[0u].last_update_ms32;
            for (io::u32 i = 1u; i < CLIENT_REGION_CACHE_CAP; ++i) {
                if (cache[i].last_update_ms32 < oldest) {
                    oldest = cache[i].last_update_ms32;
                    idx = i;
                }
            }
        } else if (io_count < CLIENT_REGION_CACHE_CAP) {
            ++io_count;
        }
        cache[idx].used = true;
        cache[idx].id = entry.region_id;
        cache[idx].mana = entry.mana;
        cache[idx].instability = entry.instability;
        cache[idx].decay = entry.decay;
        cache[idx].bands = entry.bands;
        cache[idx].flags = entry.flags;
        cache[idx].last_update_ms32 = now_ms32;
    }

    inline void ApplyIncomingRegionState(const ge::net::RegionStateSample& sample) noexcept {
        const io::u32 now_ms32 = static_cast<io::u32>(io::monotonic_ms() & 0xFFFFFFFFull);
        region_state_lock.lock();
        io::u32 neighbors = 0u;
        io::u32 vertical_neighbors = 0u;
        bool has_current = false;
        for (io::u32 i = 0u; i < sample.count && i < ge::net::REGION_SYNC_MAX_ENTRIES; ++i) {
            const ge::net::RegionEntrySample& e = sample.entries[i];
            UpsertRegionStateEntry(io::view<ClientRegionStateEntry>{ region_state_cache, CLIENT_REGION_CACHE_CAP },
                                   region_state_count, e, now_ms32);
            if ((e.flags & ge::net::REGION_ENTRY_FLAG_CURRENT) != 0u) {
                has_current = true;
                region_current_id = e.region_id;
                region_current_mana = e.mana;
                region_current_instability = e.instability;
                region_current_decay = e.decay;
                region_current_bands = e.bands;
            } else if ((e.flags & ge::net::REGION_ENTRY_FLAG_NEIGHBOR) != 0u) {
                ++neighbors;
                if ((e.flags & ge::net::REGION_ENTRY_FLAG_VERTICAL) != 0u)
                    ++vertical_neighbors;
            }
        }
        if (!has_current && sample.count > 0u) {
            const ge::net::RegionEntrySample& e = sample.entries[0];
            region_current_id = e.region_id;
            region_current_mana = e.mana;
            region_current_instability = e.instability;
            region_current_decay = e.decay;
            region_current_bands = e.bands;
        }
        region_neighbor_count = static_cast<io::u8>(neighbors > 255u ? 255u : neighbors);
        region_vertical_neighbor_count = static_cast<io::u8>(vertical_neighbors > 255u ? 255u : vertical_neighbors);
        region_last_sync_ms32 = now_ms32;
        region_state_lock.unlock();
    }

    inline void GetActiveRegionVisual(float& out_mana, float& out_instability, float& out_decay) noexcept {
        const ge::region::RegionId guessed = ge::region::region_id_from_world(
            floor_to_i32(camera.position[0]),
            floor_to_i32(camera.position[1]),
            floor_to_i32(camera.position[2]));
        out_mana = 1.0f;
        out_instability = 0.0f;
        out_decay = 0.0f;
        region_state_lock.lock();
        bool found = false;
        if (region_current_id == guessed && region_current_id != 0ull) {
            out_mana = ge::region::to_norm(region_current_mana);
            out_instability = ge::region::to_norm(region_current_instability);
            out_decay = ge::region::to_norm(region_current_decay);
            found = true;
        } else {
            for (io::u32 i = 0u; i < CLIENT_REGION_CACHE_CAP; ++i) {
                const ClientRegionStateEntry& e = region_state_cache[i];
                if (!e.used || e.id != guessed) continue;
                out_mana = ge::region::to_norm(e.mana);
                out_instability = ge::region::to_norm(e.instability);
                out_decay = ge::region::to_norm(e.decay);
                found = true;
                break;
            }
        }
        if (!found && region_current_id != 0ull) {
            out_mana = ge::region::to_norm(region_current_mana);
            out_instability = ge::region::to_norm(region_current_instability);
            out_decay = ge::region::to_norm(region_current_decay);
        }
        region_state_lock.unlock();
    }

    inline void ClearPlayerRoster() noexcept {
        player_roster_lock.lock();
        player_roster_head = 0u;
        player_roster_count = 0u;
        for (io::u32 i = 0u; i < ge::net::PLAYER_ROSTER_CLIENT_CAP; ++i) {
            player_roster_name_len[i] = 0u;
            player_roster_server_index[i] = 0xFFFFu;
        }
        if (player_roster_names)
            for (io::u32 i = 0u; i < ge::net::PLAYER_ROSTER_CLIENT_CAP * ge::net::PLAYER_NICK_BYTES; ++i)
                player_roster_names[i] = '\0';
        if (player_roster_ping)
            for (io::u32 i = 0u; i < (ge::net::PLAYER_ROSTER_CLIENT_CAP + 1u) / 2u; ++i)
                player_roster_ping[i] = 0u;
        player_roster_lock.unlock();
    }

    inline void RemovePlayerRosterByServerIndex(io::u16 server_index) noexcept {
        if (!player_roster_names) return;
        player_roster_lock.lock();
        io::u32 remove_pos = ge::net::PLAYER_ROSTER_CLIENT_CAP;
        for (io::u32 i = 0u; i < player_roster_count; ++i) {
            const io::u32 slot = (player_roster_head + i) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
            if (player_roster_server_index[slot] == server_index) {
                remove_pos = i;
                break;
            }
        }
        if (remove_pos < ge::net::PLAYER_ROSTER_CLIENT_CAP) {
            for (io::u32 j = remove_pos; j + 1u < player_roster_count; ++j) {
                const io::u32 src_slot = (player_roster_head + j + 1u) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
                const io::u32 dst_slot = (player_roster_head + j) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
                player_roster_server_index[dst_slot] = player_roster_server_index[src_slot];
                player_roster_name_len[dst_slot] = player_roster_name_len[src_slot];
                const char* src = player_roster_names + static_cast<io::usize>(src_slot) * ge::net::PLAYER_NICK_BYTES;
                char* dst = player_roster_names + static_cast<io::usize>(dst_slot) * ge::net::PLAYER_NICK_BYTES;
                for (io::u32 n = 0u; n < ge::net::PLAYER_NICK_BYTES; ++n)
                    dst[n] = src[n];
                SetPlayerRosterSignalAt(dst_slot, PlayerRosterSignalAt(src_slot));
            }
            const io::u32 tail_slot = (player_roster_head + player_roster_count - 1u) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
            player_roster_server_index[tail_slot] = 0xFFFFu;
            player_roster_name_len[tail_slot] = 0u;
            char* tail_name = player_roster_names + static_cast<io::usize>(tail_slot) * ge::net::PLAYER_NICK_BYTES;
            for (io::u32 n = 0u; n < ge::net::PLAYER_NICK_BYTES; ++n)
                tail_name[n] = '\0';
            SetPlayerRosterSignalAt(tail_slot, ge::net::SignalQuality::Bad);
            --player_roster_count;
        }
        player_roster_lock.unlock();
    }

    IO_NODISCARD inline ge::net::SignalQuality PlayerRosterSignalAt(io::u32 slot) const noexcept {
        if (!player_roster_ping || slot >= ge::net::PLAYER_ROSTER_CLIENT_CAP)
            return ge::net::SignalQuality::Bad;
        const io::u8 packed = player_roster_ping[slot >> 1u];
        const io::u8 nibble = ((slot & 1u) == 0u) ? (packed & 0x0Fu) : ((packed >> 4u) & 0x0Fu);
        return ge::net::signal_quality_from_nibble(nibble);
    }

    inline void SetPlayerRosterSignalAt(io::u32 slot, ge::net::SignalQuality quality) noexcept {
        if (!player_roster_ping || slot >= ge::net::PLAYER_ROSTER_CLIENT_CAP)
            return;
        const io::u8 nibble = ge::net::signal_quality_nibble(quality);
        io::u8& packed = player_roster_ping[slot >> 1u];
        if ((slot & 1u) == 0u)
            packed = static_cast<io::u8>((packed & 0xF0u) | nibble);
        else
            packed = static_cast<io::u8>((packed & 0x0Fu) | (nibble << 4u));
    }

    inline void UpdatePlayerRosterQualityByName(io::char_view name, ge::net::SignalQuality quality) noexcept {
        if (!player_roster_names || name.empty()) return;
        bool found = false;
        player_roster_lock.lock();
        for (io::u32 i = 0u; i < player_roster_count; ++i) {
            const io::u32 slot = (player_roster_head + i) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
            const io::u8 len = player_roster_name_len[slot];
            if (len != name.size()) continue;
            const char* existing = player_roster_names + static_cast<io::usize>(slot) * ge::net::PLAYER_NICK_BYTES;
            bool same = true;
            for (io::u32 n = 0u; n < len; ++n) {
                if (existing[n] != name[n]) {
                    same = false;
                    break;
                }
            }
            if (!same) continue;
            SetPlayerRosterSignalAt(slot, quality);
            found = true;
            break;
        }
        player_roster_lock.unlock();

        if (found) return;
        ge::net::PlayerRosterEntry entry{};
        entry.server_index = 0xFFFFu;
        entry.name_len = ge::net::clamp_player_name_len(static_cast<io::u8>(name.size()));
        entry.signal_quality = quality;
        for (io::u32 i = 0u; i < ge::net::PLAYER_NICK_BYTES; ++i)
            entry.name[i] = (i < entry.name_len) ? name[i] : '\0';
        UpsertPlayerRosterEntry(entry);
    }

    inline void UpsertPlayerRosterEntry(const ge::net::PlayerRosterEntry& entry) noexcept {
        if (!player_roster_names) return;
        player_roster_lock.lock();

        io::u32 slot = ge::net::PLAYER_ROSTER_CLIENT_CAP;
        for (io::u32 i = 0u; i < player_roster_count; ++i) {
            const io::u32 idx = (player_roster_head + i) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
            if (player_roster_server_index[idx] == entry.server_index) {
                slot = idx;
                break;
            }
        }

        if (slot >= ge::net::PLAYER_ROSTER_CLIENT_CAP && entry.name_len > 0u) {
            for (io::u32 i = 0u; i < player_roster_count; ++i) {
                const io::u32 idx = (player_roster_head + i) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
                if (player_roster_name_len[idx] != entry.name_len)
                    continue;
                const char* existing = player_roster_names + static_cast<io::usize>(idx) * ge::net::PLAYER_NICK_BYTES;
                bool same_name = true;
                for (io::u32 n = 0u; n < entry.name_len; ++n) {
                    if (existing[n] != entry.name[n]) {
                        same_name = false;
                        break;
                    }
                }
                if (same_name) {
                    slot = idx;
                    break;
                }
            }
        }

        if (slot >= ge::net::PLAYER_ROSTER_CLIENT_CAP) {
            if (player_roster_count < ge::net::PLAYER_ROSTER_CLIENT_CAP) {
                slot = (player_roster_head + player_roster_count) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
                ++player_roster_count;
            } else {
                slot = player_roster_head;
                player_roster_head = (player_roster_head + 1u) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
            }
        }

        const bool incoming_unknown = (entry.server_index == 0xFFFFu);
        const bool had_known_index = (player_roster_server_index[slot] != 0xFFFFu);
        if (!incoming_unknown || !had_known_index)
            player_roster_server_index[slot] = entry.server_index;
        player_roster_name_len[slot] = entry.name_len;
        char* dst = player_roster_names + static_cast<io::usize>(slot) * ge::net::PLAYER_NICK_BYTES;
        for (io::u32 i = 0u; i < ge::net::PLAYER_NICK_BYTES; ++i)
            dst[i] = (i < entry.name_len) ? entry.name[i] : '\0';
        if (!incoming_unknown || !had_known_index)
            SetPlayerRosterSignalAt(slot, entry.signal_quality);
        player_roster_lock.unlock();
    }

    inline void ObserveChatRosterEntry(const ge::net::ChatLine& line) noexcept {
        if (line.kind != ge::net::CHAT_KIND_PLAYER || line.name_len == 0u)
            return;
        ge::net::PlayerRosterEntry entry{};
        entry.server_index = 0xFFFFu;
        entry.name_len = ge::net::clamp_player_name_len(line.name_len);
        entry.signal_quality = ge::net::SignalQuality::Bad;
        for (io::u32 i = 0u; i < ge::net::PLAYER_NICK_BYTES; ++i)
            entry.name[i] = (i < entry.name_len) ? line.name[i] : '\0';
        UpsertPlayerRosterEntry(entry);
    }

    inline void ApplyPlayerRosterPage(const ge::net::PlayerRosterPage& page) noexcept {
        if ((page.flags & ge::net::PLAYER_ROSTER_PAGE_FLAG_RESET) != 0u)
            ClearPlayerRoster();
        const io::u8 count = (page.count > ge::net::PLAYER_ROSTER_PAGE_MAX_ENTRIES)
            ? static_cast<io::u8>(ge::net::PLAYER_ROSTER_PAGE_MAX_ENTRIES) : page.count;
        for (io::u32 i = 0u; i < count; ++i)
            UpsertPlayerRosterEntry(page.entries[i]);
    }

    inline void SendPlayerRosterSelfUpdate(io::u64 now_ms) noexcept {
        if (net_state.load() != 2u || !net_loop) return;
        if (player_name_len == 0u) return;
        const io::u32 ping_ms = net_ping_ms.load();
        const ge::net::SignalQuality quality = ge::net::signal_quality_from_ping_ms(ping_ms);
        const io::u8 quality_nibble = ge::net::signal_quality_nibble(quality);
        if (net_last_roster_quality == quality_nibble && now_ms - net_last_roster_report_ms < 2000u)
            return;

        ge::net::PlayerRosterSelf sample{};
        sample.name_len = static_cast<io::u8>(player_name_len > ge::net::PLAYER_NICK_BYTES ? ge::net::PLAYER_NICK_BYTES : player_name_len);
        sample.signal_quality = quality;
        for (io::u32 i = 0u; i < ge::net::PLAYER_NICK_BYTES; ++i)
            sample.name[i] = (i < sample.name_len) ? player_name_utf8[i] : '\0';
        ge::net::C2S_PlayerRosterSelf wire{};
        ge::net::encode_c2s_player_roster_self(sample, wire);
        (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_PLAYER_ROSTER_SELF, io::UdpChan::Reliable,
                                     io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
        net_last_roster_report_ms = now_ms;
        net_last_roster_quality = quality_nibble;
    }

    inline void RequestPlayerRoster(io::u16 start_index, io::u16 max_entries, io::u64 now_ms) noexcept {
        if (net_state.load() != 2u || !net_loop) return;
        ge::net::PlayerRosterRequest req{};
        req.start_index = start_index;
        req.max_entries = max_entries;
        ge::net::C2S_PlayerRosterRequest wire{};
        ge::net::encode_c2s_player_roster_request(req, wire);
        (void)net_loop->send_to_peer(net_connected_endpoint, ge::net::PK_C2S_PLAYER_ROSTER_REQUEST, io::UdpChan::Reliable,
                                     io::byte_view{ reinterpret_cast<const io::u8*>(&wire), sizeof(wire) }, now_ms);
    }

    IO_NODISCARD static inline float NormalizeYawDegrees(float angle) noexcept {
        while (angle > 180.f) angle -= 360.f;
        while (angle < -180.f) angle += 360.f;
        return angle;
    }

    IO_NODISCARD static inline float LerpYawDegrees(float from, float to, float alpha) noexcept {
        from = NormalizeYawDegrees(from);
        to = NormalizeYawDegrees(to);
        float delta = to - from;
        if (delta > 180.f) delta -= 360.f;
        if (delta < -180.f) delta += 360.f;
        return NormalizeYawDegrees(from + delta * alpha);
    }

    IO_NODISCARD static inline float YawDeltaDegrees(float from, float to) noexcept {
        from = NormalizeYawDegrees(from);
        to = NormalizeYawDegrees(to);
        float delta = to - from;
        if (delta > 180.f) delta -= 360.f;
        if (delta < -180.f) delta += 360.f;
        return delta;
    }

    inline void ClearRemotePlayers() noexcept {
        remote_players_lock.lock();
        if (remote_players)
            for (io::u32 i = 0u; i < REMOTE_PLAYER_CAP; ++i)
                remote_players[i] = {};
        remote_players_lock.unlock();
    }

    inline void RemoveRemotePlayerByServerIndex(io::u16 server_index) noexcept {
        if (!remote_players) return;
        remote_players_lock.lock();
        for (io::u32 i = 0u; i < REMOTE_PLAYER_CAP; ++i) {
            RemotePlayerVisual& rp = remote_players[i];
            if (!rp.active) continue;
            if (rp.server_index != server_index) continue;
            rp = {};
            break;
        }
        remote_players_lock.unlock();
    }

    inline void ApplyRemotePlayerPose(const ge::net::RemotePlayerPoseSample& sample) noexcept {
        if (!remote_players) return;
        const io::u32 now_ms = static_cast<io::u32>(io::monotonic_ms());
        remote_players_lock.lock();
        io::u32 slot = REMOTE_PLAYER_CAP;
        io::u32 oldest_slot = 0u;
        io::u32 oldest_ms = 0xFFFFFFFFu;
        for (io::u32 i = 0u; i < REMOTE_PLAYER_CAP; ++i) {
            RemotePlayerVisual& rp = remote_players[i];
            if (!rp.active) {
                slot = i;
                break;
            }
            if (rp.server_index == sample.server_index) {
                slot = i;
                break;
            }
            if (rp.last_update_ms < oldest_ms) {
                oldest_ms = rp.last_update_ms;
                oldest_slot = i;
            }
        }
        if (slot >= REMOTE_PLAYER_CAP)
            slot = oldest_slot;

        RemotePlayerVisual& rp = remote_players[slot];
        const bool is_new = (!rp.active || rp.server_index != sample.server_index);
        const io::u8 prev_state = rp.state;
        const float dx = sample.x - rp.raw_x;
        const float dy = sample.y - rp.raw_y;
        const float dz = sample.z - rp.raw_z;
        const float teleport_d2 = dx * dx + dy * dy + dz * dz;
        float sample_dt = 0.08f;
        if (!is_new && rp.last_update_ms != 0u && now_ms > rp.last_update_ms) {
            const io::u32 dt_ms = now_ms - rp.last_update_ms;
            sample_dt = static_cast<float>(dt_ms) * 0.001f;
            if (sample_dt < 0.01f) sample_dt = 0.01f;
            if (sample_dt > 0.25f) sample_dt = 0.25f;
            rp.vel_x = (sample.x - rp.raw_x) / sample_dt;
            rp.vel_y = (sample.y - rp.raw_y) / sample_dt;
            rp.vel_z = (sample.z - rp.raw_z) / sample_dt;
        } else {
            rp.vel_x = 0.f;
            rp.vel_y = 0.f;
            rp.vel_z = 0.f;
        }

        const float predict_horizon_xz = clampf(sample_dt * 0.60f + 0.035f, 0.02f, 0.10f);
        const float predict_horizon_y = clampf(sample_dt * 0.18f, 0.0f, 0.025f);
        rp.active = true;
        rp.server_index = sample.server_index;
        rp.state = sample.state;
        rp.action_flags = sample.action_flags;
        rp.held_item = ge::item::valid(sample.held_item) ? sample.held_item : ge::item::Id::None;
        rp.prev_update_ms = rp.last_update_ms;
        rp.last_update_ms = now_ms;
        rp.raw_x = sample.x;
        rp.raw_y = sample.y;
        rp.raw_z = sample.z;
        rp.target_x = sample.x + rp.vel_x * predict_horizon_xz;
        rp.target_y = sample.y + rp.vel_y * predict_horizon_y;
        rp.target_z = sample.z + rp.vel_z * predict_horizon_xz;
        rp.target_yaw = NormalizeYawDegrees(sample.yaw);
        if (sample.state == ge::net::PLAYER_ANIM_WALK || sample.state == ge::net::PLAYER_ANIM_RUN ||
            sample.state == ge::net::PLAYER_ANIM_CRAWL_MOVE) {
            rp.target_body_yaw = rp.target_yaw;
        } else {
            const float look_delta = YawDeltaDegrees(rp.body_yaw, rp.target_yaw);
            if (look_delta > 70.f) rp.target_body_yaw = NormalizeYawDegrees(rp.target_yaw - 70.f);
            else if (look_delta < -70.f) rp.target_body_yaw = NormalizeYawDegrees(rp.target_yaw + 70.f);
            else rp.target_body_yaw = NormalizeYawDegrees(rp.body_yaw + look_delta * 0.18f);
        }
        rp.target_pitch = clampf(sample.pitch, -89.f, 89.f);
        if (is_new || teleport_d2 > 100.f) {
            rp.x = sample.x;
            rp.y = sample.y;
            rp.z = sample.z;
            rp.yaw = rp.target_yaw;
            rp.body_yaw = rp.target_yaw;
            rp.pitch = rp.target_pitch;
            rp.anim_time = 0.f;
        } else if (sample.state != prev_state) {
            rp.anim_time = 0.f;
        }
        remote_players_lock.unlock();
    }

    inline void UpdateRemotePlayers(float dt) noexcept {
        if (!remote_players) return;
        const io::u32 now_ms = static_cast<io::u32>(io::monotonic_ms());
        remote_players_lock.lock();
        for (io::u32 i = 0u; i < REMOTE_PLAYER_CAP; ++i) {
            RemotePlayerVisual& rp = remote_players[i];
            if (!rp.active) continue;
            if (now_ms - rp.last_update_ms > 5000u) {
                rp = {};
                continue;
            }
            const float alpha = clampf(dt * 11.f, 0.f, 1.f);
            const float alpha_y = clampf(dt * 17.f, 0.f, 1.f);
            const float look_alpha = clampf(dt * 14.f, 0.f, 1.f);
            const bool moving = (rp.state == ge::net::PLAYER_ANIM_WALK || rp.state == ge::net::PLAYER_ANIM_RUN ||
                                 rp.state == ge::net::PLAYER_ANIM_CRAWL_MOVE);
            const float body_alpha = clampf(dt * (moving ? 8.0f : 2.5f), 0.f, 1.f);
            const float ex_delay = 0.10f;
            const float ex_cap = 0.08f;
            const float since_update = static_cast<float>(now_ms - rp.last_update_ms) * 0.001f;
            float ex = 0.f;
            if (since_update > ex_delay)
                ex = clampf(since_update - ex_delay, 0.f, ex_cap);
            const float dynamic_x = rp.target_x + rp.vel_x * ex;
            const float dynamic_z = rp.target_z + rp.vel_z * ex;
            if (absf(dynamic_x - rp.x) > 4.0f) rp.x = dynamic_x;
            if (absf(dynamic_z - rp.z) > 4.0f) rp.z = dynamic_z;
            rp.x += (rp.target_x - rp.x) * alpha;
            if (absf(rp.target_y - rp.y) > 1.2f) rp.y = rp.target_y;
            rp.y += (rp.target_y - rp.y) * alpha_y;
            rp.z += (rp.target_z - rp.z) * alpha;
            rp.yaw = LerpYawDegrees(rp.yaw, rp.target_yaw, look_alpha);
            rp.body_yaw = LerpYawDegrees(rp.body_yaw, rp.target_body_yaw, body_alpha);
            float head_delta = YawDeltaDegrees(rp.body_yaw, rp.yaw);
            if (head_delta > 78.f)
                rp.body_yaw = NormalizeYawDegrees(rp.yaw - 78.f);
            else if (head_delta < -78.f)
                rp.body_yaw = NormalizeYawDegrees(rp.yaw + 78.f);
            rp.pitch += (rp.target_pitch - rp.pitch) * alpha;
            rp.anim_time += dt;
        }
        remote_players_lock.unlock();
    }

    IO_NODISCARD inline bool RosterNameByServerIndex(io::u16 server_index, io::char_view& out_name) noexcept {
        out_name = {};
        if (!player_roster_names) return false;
        bool ok = false;
        player_roster_lock.lock();
        for (io::u32 i = 0u; i < player_roster_count; ++i) {
            const io::u32 slot = (player_roster_head + i) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
            if (player_roster_server_index[slot] != server_index)
                continue;
            const io::u8 len = player_roster_name_len[slot];
            if (len == 0u) break;
            const char* src = player_roster_names + static_cast<io::usize>(slot) * ge::net::PLAYER_NICK_BYTES;
            out_name = io::char_view{ src, len };
            ok = true;
            break;
        }
        player_roster_lock.unlock();
        return ok;
    }

    inline void ResetTargetBreak() noexcept {
        target_break_progress = 0.f;
        target_break_wx = 0;
        target_break_wy = 0;
        target_break_wz = 0;
    }

    inline void UpdateTargetBlockState(float dt) noexcept {
        target_block_valid = false;
        if (screen != ScreenState::InGame || !chunk_world_ready) {
            ResetTargetBreak();
            return;
        }

        BlockRayHit hit{};
        if (RaycastSolidBlock(hit)) {
            target_block_valid = true;
            target_block_wx = hit.wx;
            target_block_wy = hit.wy;
            target_block_wz = hit.wz;
            target_block_id = ge::voxel::block_index(voxel_world.get_world_block(hit.wx, hit.wy, hit.wz));
        }

        const bool can_break = target_block_valid && !isCursorVisible() && !chat_open && !inventory_open && !player_dead;
        if (!can_break || !mouse_left_down) {
            ResetTargetBreak();
            return;
        }

        if (use_fly) {
            static constexpr io::u64 GODMODE_BREAK_REPEAT_MS = 90u;
            const io::u64 now_ms = io::monotonic_ms();
            if (now_ms >= target_break_last_submit_ms &&
                (now_ms - target_break_last_submit_ms) >= GODMODE_BREAK_REPEAT_MS) {
                if (SubmitBlockEdit(target_block_wx, target_block_wy, target_block_wz, ge::voxel::BlockId::Air, 0u))
                    target_break_last_submit_ms = now_ms;
            }
            target_break_progress = 0.f;
            return;
        }

        const ge::item::Stack& held_stack = SelectedHotbarStack();
        const bool held_empty = ge::item::is_empty(held_stack);
        const bool held_is_dagger = !held_empty && held_stack.id == ge::item::Id::RustyDagger;
        const bool held_is_ward = !held_empty &&
            ge::item::def(held_stack.id).category == ge::item::Category::SpellingWards;
        if (!held_is_dagger && !held_is_ward) {
            ResetTargetBreak();
            return;
        }
        const ge::voxel::BlockId target_id = (target_block_id < ge::voxel::BLOCK_COUNT)
            ? static_cast<ge::voxel::BlockId>(target_block_id)
            : ge::voxel::BlockId::Air;
        if (held_is_dagger && !ge::build::dagger_can_break(block_build_profile, target_id)) {
            ResetTargetBreak();
            return;
        }

        if (target_break_wx != target_block_wx ||
            target_break_wy != target_block_wy ||
            target_break_wz != target_block_wz) {
            target_break_wx = target_block_wx;
            target_break_wy = target_block_wy;
            target_break_wz = target_block_wz;
            target_break_progress = 0.f;
            return;
        }

        target_break_progress = clampf(target_break_progress + dt * (1.f / 0.55f), 0.f, 1.f);
        if (target_break_progress >= 1.f) {
            static constexpr io::u64 BREAK_SUBMIT_RETRY_MS = 90u;
            const io::u64 now_ms = io::monotonic_ms();
            if (now_ms >= target_break_last_submit_ms &&
                (now_ms - target_break_last_submit_ms) >= BREAK_SUBMIT_RETRY_MS) {
                const bool submitted =
                    SubmitBlockEdit(target_block_wx, target_block_wy, target_block_wz, ge::voxel::BlockId::Air, 0u);
                target_break_last_submit_ms = now_ms;
                if (submitted) {
                    ResetTargetBreak();
                } else {
                    // Keep visible cracking and retry submit shortly; do not force player to restart breaking.
                    target_break_progress = 0.92f;
                }
            }
        }
    }

    static inline lm::mat4 OrthoMatrix(float left, float right, float bottom, float top, float near_z, float far_z) noexcept {
        lm::mat4 out = lm::mat4_identity();
        out[0][0] = 2.f / (right - left);
        out[1][1] = 2.f / (top - bottom);
        out[2][2] = -2.f / (far_z - near_z);
        out[3][0] = -(right + left) / (right - left);
        out[3][1] = -(top + bottom) / (top - bottom);
        out[3][2] = -(far_z + near_z) / (far_z - near_z);
        return out;
    }

    IO_NODISCARD static inline ge::item::SlotRegion InventoryRegionForTab(InventoryUiTab tab) noexcept {
        switch (tab) {
        case InventoryUiTab::General: return ge::item::SlotRegion::General;
        case InventoryUiTab::Blocks: return ge::item::SlotRegion::Blocks;
        case InventoryUiTab::Spells: return ge::item::SlotRegion::Spells;
        default:
            return ge::item::SlotRegion::General;
        }
    }

    IO_NODISCARD static inline io::char_view InventoryTabLabel(InventoryUiTab tab) noexcept {
        switch (tab) {
        case InventoryUiTab::General: return "General";
        case InventoryUiTab::Blocks: return "Blocks";
        case InventoryUiTab::Spells: return "Spells";
        default:
            return "General";
        }
    }

    IO_NODISCARD static inline io::char_view InventoryTabLabel(ge::item::Category tab) noexcept {
        switch (tab) {
        case ge::item::Category::Blocks: return "Blocks";
        case ge::item::Category::Consumables: return "Consumables";
        case ge::item::Category::SpellingWards: return "Spelling Wards";
        case ge::item::Category::Spells: return "Spells";
        case ge::item::Category::Materials:
        default:
            return "Materials";
        }
    }

    template<io::usize N>
    static inline void BuildSlotLabel(io::StackOut<N>& out,
                                      const ge::item::Stack& stack,
                                      io::u32 slot_number,
                                      bool show_slot_number) noexcept {
        out.reset();
        if (show_slot_number)
            out << slot_number << " ";
        if (ge::item::is_empty(stack)) {
            out << "--";
            return;
        }
        out << ge::item::short_name(stack.id) << " " << stack.count;
        if (ge::item::decays(stack.id) && ge::item::freshness_band(stack) == ge::item::FreshnessBand::Rotten)
            out << " !";
    }

    static inline bool IsSpaceChar(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static inline io::char_view TrimChatText(io::char_view text) noexcept {
        io::usize a = 0;
        io::usize b = text.size();
        while (a < b && IsSpaceChar(text[a])) ++a;
        while (b > a && IsSpaceChar(text[b - 1])) --b;
        return text.slice(a, b - a);
    }

    static constexpr io::u64 CHAT_TEXT_FIELD_ID = 97001u;

    static inline char ToLowerAscii(char ch) noexcept {
        if (ch >= 'A' && ch <= 'Z')
            return static_cast<char>(ch - 'A' + 'a');
        return ch;
    }

    static inline bool ChatCommandEq(io::char_view text, io::char_view command) noexcept {
        if (text.size() != command.size()) return false;
        for (io::usize i = 0; i < text.size(); ++i)
            if (ToLowerAscii(text[i]) != ToLowerAscii(command[i]))
                return false;
        return true;
    }

    struct ChatCommandSpec {
        const char* name = "";
        const char* description = "";
        const char* arg_help[4]{};
    };

    struct ChatTokenAutocompleteSpec {
        const char* token = "";
        const char* description = "";
        ge::item::Id icon_item = ge::item::Id::None;
    };

    static constexpr ChatCommandSpec CHAT_COMMAND_SPECS[]{
        { "/help", "Show available commands", { "", "", "", "" } },
        { "/give", "Give item to player", { "<arg1> @nickname", "<arg2> item/block name", "", "" } },
        { "/pos", "Show your coordinates", { "", "", "", "" } },
        { "/tp", "Teleport to coordinates", { "<arg1> x", "<arg2> y", "<arg3> z", "" } },
        { "/setblock", "Set a block in world", { "<arg1> x", "<arg2> y", "<arg3> z", "<arg4> block name/id" } },
        { "/watersource", "Toggle water source", { "<arg1> x", "<arg2> y", "<arg3> z", "<arg4> on|off" } },
        { "/settime", "Set time of day", { "<arg1> day light - value between 1-59; night during 60-100", "", "", "" } },
        { "/godmode", "Toggle god mode", { "", "", "", "" } },
        { "/noclip", "Toggle noclip mode", { "", "", "", "" } }
    };
    static constexpr io::u32 CHAT_COMMAND_SPEC_COUNT =
        static_cast<io::u32>(sizeof(CHAT_COMMAND_SPECS) / sizeof(CHAT_COMMAND_SPECS[0]));

    static constexpr ChatTokenAutocompleteSpec CHAT_BLOCK_SUGGESTIONS[]{
        { "air", "block", ge::item::Id::None },
        { "grass", "block", ge::item::Id::GrassBlock },
        { "dirt", "block", ge::item::Id::DirtBlock },
        { "stone", "block", ge::item::Id::StoneBlock },
        { "sand", "block", ge::item::Id::SandBlock },
        { "water", "block", ge::item::Id::WaterBucket },
        { "water_dark", "block", ge::item::Id::WaterBucket },
        { "blood", "block", ge::item::Id::BloodBucket },
        { "blood_dark", "block", ge::item::Id::BloodBucket },
        { "slime", "block", ge::item::Id::SlimeBucket },
        { "slime_dark", "block", ge::item::Id::SlimeBucket },
        { "snow", "block", ge::item::Id::None },
        { "log", "block", ge::item::Id::LogBlock },
        { "leaves", "block", ge::item::Id::LeavesBlock },
        { "book", "block", ge::item::Id::SpellWard }
    };

    static constexpr ChatTokenAutocompleteSpec CHAT_GIVE_ITEM_SUGGESTIONS[]{
        { "grass", "item", ge::item::Id::GrassBlock },
        { "dirt", "item", ge::item::Id::DirtBlock },
        { "stone", "item", ge::item::Id::StoneBlock },
        { "sand", "item", ge::item::Id::SandBlock },
        { "log", "item", ge::item::Id::LogBlock },
        { "leaves", "item", ge::item::Id::LeavesBlock },
        { "potato", "item", ge::item::Id::Potato },
        { "spellward", "item", ge::item::Id::SpellWard },
        { "spellbolt", "spell", ge::item::Id::SpellBolt },
        { "spelldig", "spell", ge::item::Id::SpellDig },
        { "spellburst", "spell", ge::item::Id::SpellBurst },
        { "spellbeam", "spell", ge::item::Id::SpellBeam },
        { "spellorb", "spell", ge::item::Id::SpellOrb },
        { "spellmine", "spell", ge::item::Id::SpellMine },
        { "spellshieldpulse", "spell", ge::item::Id::SpellShieldPulse },
        { "spellmark", "spell", ge::item::Id::SpellMark },
        { "spellpull", "spell", ge::item::Id::SpellPull },
        { "spellblinkstep", "spell", ge::item::Id::SpellBlinkStep }
    };

    static inline io::char_view ChatPopToken(io::char_view& text) noexcept {
        text = TrimChatText(text);
        if (text.empty()) return {};
        io::usize n = 0;
        while (n < text.size() && !IsSpaceChar(text[n])) ++n;
        const io::char_view token = text.slice(0u, n);
        text = text.slice(n, text.size() - n);
        return token;
    }

    IO_NODISCARD static inline io::u32 ChatFuzzyScore(io::char_view typed, io::char_view candidate) noexcept {
        if (typed.empty()) return 0u;
        io::usize ti = 0u;
        io::u32 penalty = 0u;
        for (io::usize ci = 0u; ci < candidate.size(); ++ci) {
            if (ti < typed.size() && ToLowerAscii(candidate[ci]) == ToLowerAscii(typed[ti])) {
                if (ci != ti) penalty += 2u;
                ++ti;
            } else {
                ++penalty;
            }
        }
        if (ti < typed.size()) return 0xFFFFFFFFu;
        if (candidate.size() >= typed.size()) penalty += static_cast<io::u32>(candidate.size() - typed.size());
        return penalty;
    }

    IO_NODISCARD static inline bool IsSignedIntegerToken(io::char_view tok) noexcept {
        if (tok.empty()) return false;
        io::usize i = 0u;
        if (tok[0] == '+' || tok[0] == '-') {
            if (tok.size() == 1u) return false;
            i = 1u;
        }
        for (; i < tok.size(); ++i)
            if (tok[i] < '0' || tok[i] > '9')
                return false;
        return true;
    }

    IO_NODISCARD static inline bool IsDotToken(io::char_view tok) noexcept {
        return tok.size() == 1u && tok[0] == '.';
    }

    IO_NODISCARD static inline io::u32 ApproxPingMsFromQuality(ge::net::SignalQuality q) noexcept {
        switch (q) {
        case ge::net::SignalQuality::Excellent: return 60u;
        case ge::net::SignalQuality::Good: return 110u;
        case ge::net::SignalQuality::Okay: return 180u;
        default: return 220u;
        }
    }

    struct ChatParsedCommand {
        io::char_view command{};
        io::char_view args[6]{};
        io::u32 arg_count = 0u;
        bool trailing_space = false;
    };

    IO_NODISCARD static inline ChatParsedCommand ParseChatCommand(io::char_view text) noexcept {
        ChatParsedCommand out{};
        text = TrimChatText(text);
        if (text.empty() || text[0] != '/')
            return out;
        out.trailing_space = !text.empty() && IsSpaceChar(text[text.size() - 1u]);
        io::char_view tail = text;
        out.command = ChatPopToken(tail);
        while (out.arg_count < 6u) {
            io::char_view tok = ChatPopToken(tail);
            if (tok.empty()) break;
            out.args[out.arg_count++] = tok;
        }
        return out;
    }

    inline void PushChatSuggestion(io::char_view text, io::char_view desc, ge::item::Id icon_item = ge::item::Id::None) noexcept {
        if (chat_suggestion_count >= 8u || text.empty()) return;
        for (io::u32 i = 0u; i < chat_suggestion_count; ++i) {
            if (chat_suggestion_text_len[i] != text.size()) continue;
            bool same = true;
            for (io::u32 n = 0u; n < text.size(); ++n) {
                if (chat_suggestion_text[i][n] != text[n]) {
                    same = false;
                    break;
                }
            }
            if (same) return;
        }
        const io::u32 slot = chat_suggestion_count++;
        CopySmallText(chat_suggestion_text[slot], chat_suggestion_text_len[slot], sizeof(chat_suggestion_text[slot]), text);
        CopySmallText(chat_suggestion_desc[slot], chat_suggestion_desc_len[slot], sizeof(chat_suggestion_desc[slot]), desc);
        chat_suggestion_icon_item[slot] = icon_item;
    }

    inline void AddBestCommandSuggestions(io::char_view command_token) noexcept {
        io::u32 best_idx[8]{};
        io::u32 best_score[8]{};
        for (io::u32 i = 0u; i < 8u; ++i) best_score[i] = 0xFFFFFFFFu;

        for (io::u32 i = 0u; i < CHAT_COMMAND_SPEC_COUNT; ++i) {
            const io::char_view candidate = CHAT_COMMAND_SPECS[i].name;
            const io::u32 score = ChatFuzzyScore(command_token, candidate);
            if (score == 0xFFFFFFFFu) continue;
            for (io::u32 pos = 0u; pos < 8u; ++pos) {
                if (score >= best_score[pos]) continue;
                for (io::u32 shift = 7u; shift > pos; --shift) {
                    best_score[shift] = best_score[shift - 1u];
                    best_idx[shift] = best_idx[shift - 1u];
                }
                best_score[pos] = score;
                best_idx[pos] = i;
                break;
            }
        }

        for (io::u32 i = 0u; i < 8u; ++i) {
            if (best_score[i] == 0xFFFFFFFFu) break;
            PushChatSuggestion(CHAT_COMMAND_SPECS[best_idx[i]].name,
                               CHAT_COMMAND_SPECS[best_idx[i]].description);
        }
    }

    inline void AddBestTokenSuggestionsFromTable(const ChatTokenAutocompleteSpec* table, io::u32 table_count,
                                                 io::char_view typed, bool typed_with_at = false) noexcept {
        io::u32 best_idx[8]{};
        io::u32 best_score[8]{};
        for (io::u32 i = 0u; i < 8u; ++i) best_score[i] = 0xFFFFFFFFu;
        const io::char_view pure_typed = (typed_with_at && !typed.empty() && typed[0] == '@')
            ? typed.slice(1u, typed.size() - 1u)
            : typed;

        for (io::u32 i = 0u; i < table_count; ++i) {
            const io::char_view candidate = table[i].token;
            const io::u32 score = ChatFuzzyScore(pure_typed, candidate);
            if (score == 0xFFFFFFFFu) continue;
            for (io::u32 pos = 0u; pos < 8u; ++pos) {
                if (score >= best_score[pos]) continue;
                for (io::u32 shift = 7u; shift > pos; --shift) {
                    best_score[shift] = best_score[shift - 1u];
                    best_idx[shift] = best_idx[shift - 1u];
                }
                best_score[pos] = score;
                best_idx[pos] = i;
                break;
            }
        }

        for (io::u32 i = 0u; i < 8u; ++i) {
            if (best_score[i] == 0xFFFFFFFFu) break;
            io::StackOut<64> token{};
            if (typed_with_at) token << "@";
            token << table[best_idx[i]].token;
            PushChatSuggestion(token.view(), table[best_idx[i]].description, table[best_idx[i]].icon_item);
        }
    }

    inline void AddBestPlayerNameSuggestions(io::char_view typed_with_optional_at) noexcept {
        if (!player_roster_names) return;
        io::char_view typed = typed_with_optional_at;
        if (!typed.empty() && typed[0] == '@')
            typed = typed.slice(1u, typed.size() - 1u);

        struct Candidate {
            io::u32 score = 0xFFFFFFFFu;
            io::u32 slot = ge::net::PLAYER_ROSTER_CLIENT_CAP;
        };
        Candidate best[8]{};

        player_roster_lock.lock();
        for (io::u32 i = 0u; i < player_roster_count; ++i) {
            const io::u32 slot = (player_roster_head + i) % ge::net::PLAYER_ROSTER_CLIENT_CAP;
            const io::u8 len = player_roster_name_len[slot];
            if (len == 0u) continue;
            const char* name_ptr = player_roster_names + static_cast<io::usize>(slot) * ge::net::PLAYER_NICK_BYTES;
            const io::char_view name{ name_ptr, len };
            const io::u32 score = ChatFuzzyScore(typed, name);
            if (score == 0xFFFFFFFFu) continue;
            for (io::u32 pos = 0u; pos < 8u; ++pos) {
                if (score >= best[pos].score) continue;
                for (io::u32 shift = 7u; shift > pos; --shift)
                    best[shift] = best[shift - 1u];
                best[pos].score = score;
                best[pos].slot = slot;
                break;
            }
        }

        for (io::u32 i = 0u; i < 8u; ++i) {
            if (best[i].slot >= ge::net::PLAYER_ROSTER_CLIENT_CAP) continue;
            const io::u32 slot = best[i].slot;
            const io::u8 len = player_roster_name_len[slot];
            const char* name_ptr = player_roster_names + static_cast<io::usize>(slot) * ge::net::PLAYER_NICK_BYTES;
            io::StackOut<96> token{};
            token << "@" << io::char_view{ name_ptr, len };
            PushChatSuggestion(token.view(), "player");
        }
        player_roster_lock.unlock();
    }

    inline void FillCommandArgHelp(io::char_view command_token) noexcept {
        for (io::u32 i = 0u; i < CHAT_COMMAND_SPEC_COUNT; ++i) {
            const ChatCommandSpec& spec = CHAT_COMMAND_SPECS[i];
            if (!ChatCommandEq(command_token, spec.name)) continue;
            for (io::u32 arg = 0u; arg < 4u; ++arg) {
                const io::char_view help = spec.arg_help[arg];
                if (help.empty()) continue;
                CopySmallText(chat_arg_help[arg], chat_arg_help_len[arg], sizeof(chat_arg_help[arg]), help);
                ++chat_arg_help_count;
            }
            break;
        }
    }

    inline void ResetChatSuggestions() noexcept {
        chat_suggestion_count = 0u;
        chat_suggestion_selected = 0u;
        chat_arg_help_count = 0u;
        for (io::u32 i = 0u; i < 8u; ++i) {
            chat_suggestion_text_len[i] = 0u;
            chat_suggestion_desc_len[i] = 0u;
            chat_suggestion_text[i][0] = '\0';
            chat_suggestion_desc[i][0] = '\0';
            chat_suggestion_icon_item[i] = ge::item::Id::None;
        }
        for (io::u32 i = 0u; i < 4u; ++i) {
            chat_arg_help_len[i] = 0u;
            chat_arg_help[i][0] = '\0';
        }
    }

    inline void CopySmallText(char* dst, io::u8& out_len, io::usize dst_cap, io::char_view text) noexcept {
        out_len = 0u;
        if (dst_cap == 0u) return;
        const io::usize limit = dst_cap - 1u;
        for (io::usize i = 0u; i < text.size() && i < limit; ++i)
            dst[out_len++] = text[i];
        dst[out_len] = '\0';
    }

    inline void UpdateChatSuggestions() noexcept {
        ResetChatSuggestions();
        if (!chat_open) return;
        const io::char_view text = TrimChatText(io::char_view{ chat_input_utf8, chat_input_len });
        if (text.empty() || text[0] != '/') return;

        const ChatParsedCommand parsed = ParseChatCommand(text);
        if (parsed.command.empty()) return;

        io::u32 exact_command_idx = CHAT_COMMAND_SPEC_COUNT;
        for (io::u32 i = 0u; i < CHAT_COMMAND_SPEC_COUNT; ++i) {
            if (ChatCommandEq(parsed.command, CHAT_COMMAND_SPECS[i].name)) {
                exact_command_idx = i;
                break;
            }
        }

        if (exact_command_idx >= CHAT_COMMAND_SPEC_COUNT) {
            AddBestCommandSuggestions(parsed.command);
            if (chat_suggestion_selected >= chat_suggestion_count)
                chat_suggestion_selected = 0u;
            return;
        }

        FillCommandArgHelp(parsed.command);

        const bool trailing = parsed.trailing_space;
        io::u32 focus_arg_index = 0u;
        io::char_view focus_token{};
        if (parsed.arg_count == 0u) {
            focus_arg_index = 0u;
        } else if (trailing) {
            focus_arg_index = parsed.arg_count;
        } else {
            focus_arg_index = parsed.arg_count - 1u;
            focus_token = parsed.args[focus_arg_index];
        }

        if (ChatCommandEq(parsed.command, "/give")) {
            if (focus_arg_index == 0u) {
                AddBestPlayerNameSuggestions(focus_token);
            } else if (focus_arg_index == 1u) {
                AddBestTokenSuggestionsFromTable(
                    CHAT_GIVE_ITEM_SUGGESTIONS,
                    static_cast<io::u32>(sizeof(CHAT_GIVE_ITEM_SUGGESTIONS) / sizeof(CHAT_GIVE_ITEM_SUGGESTIONS[0])),
                    focus_token);
            }
        } else if (ChatCommandEq(parsed.command, "/setblock")) {
            const bool alt_single_block_mode =
                (parsed.arg_count == 0u) ||
                (focus_arg_index == 0u && !trailing &&
                 !IsSignedIntegerToken(focus_token) && !IsDotToken(focus_token));
            if (alt_single_block_mode || focus_arg_index == 3u) {
                AddBestTokenSuggestionsFromTable(
                    CHAT_BLOCK_SUGGESTIONS,
                    static_cast<io::u32>(sizeof(CHAT_BLOCK_SUGGESTIONS) / sizeof(CHAT_BLOCK_SUGGESTIONS[0])),
                    focus_token);
            }
        } else if (ChatCommandEq(parsed.command, "/tp")) {
            if ((focus_arg_index == 0u && !focus_token.empty() && focus_token[0] == '@') ||
                (focus_arg_index == 0u && trailing && parsed.arg_count == 1u && parsed.args[0].size() > 0u && parsed.args[0][0] == '@')) {
                AddBestPlayerNameSuggestions(trailing ? parsed.args[0] : focus_token);
            }
        }

        if (chat_suggestion_selected >= chat_suggestion_count)
            chat_suggestion_selected = 0u;
    }

    inline void SelectChatSuggestionDelta(io::i32 delta) noexcept {
        if (chat_suggestion_count == 0u) return;
        io::i32 next = static_cast<io::i32>(chat_suggestion_selected) + delta;
        if (next < 0) next = static_cast<io::i32>(chat_suggestion_count) - 1;
        if (next >= static_cast<io::i32>(chat_suggestion_count)) next = 0;
        chat_suggestion_selected = static_cast<io::u32>(next);
    }

    IO_NODISCARD inline io::char_view BestAutocompleteBlockName(io::char_view typed) const noexcept {
        io::u32 best_score = 0xFFFFFFFFu;
        io::u32 best_index = 0u;
        for (io::u32 i = 0u; i < sizeof(CHAT_BLOCK_SUGGESTIONS) / sizeof(CHAT_BLOCK_SUGGESTIONS[0]); ++i) {
            const io::char_view candidate = CHAT_BLOCK_SUGGESTIONS[i].token;
            const io::u32 score = ChatFuzzyScore(typed, candidate);
            if (score < best_score) {
                best_score = score;
                best_index = i;
            }
        }
        return CHAT_BLOCK_SUGGESTIONS[best_index].token;
    }

    IO_NODISCARD inline bool BuildCanonicalCommandWithCoordinates(io::char_view input,
                                                                  io::StackOut<256>& out,
                                                                  bool fill_missing) const noexcept {
        const ChatParsedCommand parsed = ParseChatCommand(input);
        if (parsed.command.empty()) return false;

        const io::i32 px = floor_to_i32(camera.position[0]);
        const io::i32 py = floor_to_i32(camera.position[1]);
        const io::i32 pz = floor_to_i32(camera.position[2]);

        if (ChatCommandEq(parsed.command, "/tp")) {
            if (parsed.arg_count > 0u &&
                !IsSignedIntegerToken(parsed.args[0]) &&
                !IsDotToken(parsed.args[0])) {
                return false;
            }

            out.reset();
            out << "/tp";
            for (io::u32 i = 0u; i < 3u; ++i) {
                out << " ";
                if (i < parsed.arg_count) {
                    const io::char_view tok = parsed.args[i];
                    if (IsDotToken(tok)) {
                        if (i == 0u) out << px;
                        else if (i == 1u) out << py;
                        else out << pz;
                    }
                    else
                        out << tok;
                } else {
                    if (!fill_missing) return false;
                    if (i == 0u) out << px;
                    else if (i == 1u) out << py;
                    else out << pz;
                }
            }
            return true;
        }

        if (ChatCommandEq(parsed.command, "/setblock")) {
            out.reset();
            out << "/setblock";

            if (parsed.arg_count == 1u &&
                !IsSignedIntegerToken(parsed.args[0]) &&
                !IsDotToken(parsed.args[0])) {
                out << " " << px << " " << py << " " << pz << " " << parsed.args[0];
                return true;
            }

            for (io::u32 i = 0u; i < 3u; ++i) {
                out << " ";
                if (i < parsed.arg_count) {
                    const io::char_view tok = parsed.args[i];
                    if (IsDotToken(tok)) {
                        if (i == 0u) out << px;
                        else if (i == 1u) out << py;
                        else out << pz;
                    }
                    else
                        out << tok;
                } else {
                    if (!fill_missing) return false;
                    if (i == 0u) out << px;
                    else if (i == 1u) out << py;
                    else out << pz;
                }
            }

            out << " ";
            if (parsed.arg_count >= 4u) {
                out << parsed.args[3];
            } else {
                if (!fill_missing) return false;
                out << BestAutocompleteBlockName({});
            }
            return true;
        }

        return false;
    }

    inline void ApplyChatSuggestionOrAutocomplete() noexcept {
        const io::char_view text = TrimChatText(io::char_view{ chat_input_utf8, chat_input_len });
        if (text.empty() || text[0] != '/') return;

        const ChatParsedCommand parsed = ParseChatCommand(text);
        if (parsed.command.empty()) return;

        io::u32 exact_command_idx = CHAT_COMMAND_SPEC_COUNT;
        for (io::u32 i = 0u; i < CHAT_COMMAND_SPEC_COUNT; ++i) {
            if (ChatCommandEq(parsed.command, CHAT_COMMAND_SPECS[i].name)) {
                exact_command_idx = i;
                break;
            }
        }

        const bool trailing = parsed.trailing_space;
        if (chat_suggestion_count > 0u) {
            const io::char_view pick{
                chat_suggestion_text[chat_suggestion_selected],
                chat_suggestion_text_len[chat_suggestion_selected]
            };

            io::StackOut<256> ss{};
            if (exact_command_idx >= CHAT_COMMAND_SPEC_COUNT) {
                ss << pick << " ";
                SetChatInput(ss.view());
                FocusTextField(CHAT_TEXT_FIELD_ID);
                UpdateChatSuggestions();
                return;
            }

            const io::u32 focus_arg = (parsed.arg_count == 0u)
                ? 0u
                : (trailing ? parsed.arg_count : (parsed.arg_count - 1u));
            ss << parsed.command;
            const io::u32 new_arg_count = (focus_arg >= parsed.arg_count) ? (focus_arg + 1u) : parsed.arg_count;
            for (io::u32 i = 0u; i < new_arg_count && i < 6u; ++i) {
                ss << " ";
                if (i == focus_arg)
                    ss << pick;
                else if (i < parsed.arg_count)
                    ss << parsed.args[i];
            }
            ss << " ";
            SetChatInput(ss.view());
            FocusTextField(CHAT_TEXT_FIELD_ID);
            UpdateChatSuggestions();
            return;
        }

        io::StackOut<256> canonical{};
        if (BuildCanonicalCommandWithCoordinates(text, canonical, true)) {
            canonical << " ";
            SetChatInput(canonical.view());
            FocusTextField(CHAT_TEXT_FIELD_ID);
            UpdateChatSuggestions();
        }
    }

    inline void ClearChatInput() noexcept {
        chat_input_len = 0;
        chat_input_utf8[0] = '\0';
    }

    inline void SetChatInput(io::char_view text) noexcept {
        chat_input_len = 0;
        const io::usize n = (text.size() > ge::net::CHAT_TEXT_MAX) ? ge::net::CHAT_TEXT_MAX : text.size();
        for (io::usize i = 0; i < n; ++i)
            chat_input_utf8[chat_input_len++] = text[i];
        chat_input_utf8[chat_input_len] = '\0';
        SetTextFieldCursor(CHAT_TEXT_FIELD_ID, chat_input_len);
        UpdateChatSuggestions();
    }

    inline void ChatInputBackspace() noexcept {
        if (chat_input_len == 0) return;
        io::usize n = chat_input_len;
        --n;
        while (n > 0 && (static_cast<io::u8>(chat_input_utf8[n]) & 0xC0u) == 0x80u)
            --n;
        chat_input_len = n;
        chat_input_utf8[chat_input_len] = '\0';
        UpdateChatSuggestions();
    }

    inline void ChatInputAppendUtf8(io::char_view utf8) noexcept {
        if (!chat_open) return;
        for (io::usize i = 0; i < utf8.size(); ++i) {
            const char ch = utf8[i];
            if (ch == '\n' || ch == '\r') continue;
            if (static_cast<io::u8>(ch) < 0x20u) continue;
            if (chat_input_len >= ge::net::CHAT_TEXT_MAX) break;
            chat_input_utf8[chat_input_len++] = ch;
        }
        chat_input_utf8[chat_input_len] = '\0';
        UpdateChatSuggestions();
    }

    inline void PushChatHistoryEntry(io::char_view text) noexcept {
        const io::char_view trimmed = TrimChatText(text);
        if (trimmed.empty()) return;

        if (chat_history_count > 0u) {
            const io::u32 newest = (chat_history_next + CHAT_HISTORY_CAP - 1u) % CHAT_HISTORY_CAP;
            const io::u8 newest_len = chat_history_len[newest];
            if (newest_len == trimmed.size()) {
                bool same = true;
                for (io::u32 i = 0; i < newest_len; ++i)
                    if (chat_history_utf8[newest][i] != trimmed[i]) {
                        same = false;
                        break;
                    }
                if (same) {
                    chat_history_nav = -1;
                    return;
                }
            }
        }

        const io::u32 slot = chat_history_next % CHAT_HISTORY_CAP;
        const io::usize n = (trimmed.size() > ge::net::CHAT_TEXT_MAX) ? ge::net::CHAT_TEXT_MAX : trimmed.size();
        for (io::u32 i = 0; i < ge::net::CHAT_TEXT_MAX; ++i)
            chat_history_utf8[slot][i] = (i < n) ? trimmed[i] : '\0';
        chat_history_utf8[slot][n] = '\0';
        chat_history_len[slot] = static_cast<io::u8>(n);

        chat_history_next = (chat_history_next + 1u) % CHAT_HISTORY_CAP;
        if (chat_history_count < CHAT_HISTORY_CAP) ++chat_history_count;
        chat_history_nav = -1;
    }

    inline void ChatHistoryPrev() noexcept {
        if (!chat_open || chat_history_count == 0u) return;

        if (chat_history_nav < 0) {
            chat_history_draft_len = chat_input_len;
            for (io::u32 i = 0; i < ge::net::CHAT_TEXT_MAX; ++i)
                chat_history_draft_utf8[i] = (i < chat_history_draft_len) ? chat_input_utf8[i] : '\0';
            chat_history_draft_utf8[chat_history_draft_len] = '\0';
            chat_history_nav = 0;
        }
        else if (static_cast<io::u32>(chat_history_nav + 1) < chat_history_count) {
            ++chat_history_nav;
        }

        const io::u32 newest = (chat_history_next + CHAT_HISTORY_CAP - 1u) % CHAT_HISTORY_CAP;
        const io::u32 idx = (newest + CHAT_HISTORY_CAP - static_cast<io::u32>(chat_history_nav)) % CHAT_HISTORY_CAP;
        SetChatInput(io::char_view{ chat_history_utf8[idx], chat_history_len[idx] });
        FocusTextField(CHAT_TEXT_FIELD_ID);
    }

    inline void ChatHistoryNext() noexcept {
        if (!chat_open || chat_history_nav < 0) return;

        if (chat_history_nav == 0) {
            chat_history_nav = -1;
            SetChatInput(io::char_view{ chat_history_draft_utf8, chat_history_draft_len });
            FocusTextField(CHAT_TEXT_FIELD_ID);
            return;
        }

        --chat_history_nav;
        const io::u32 newest = (chat_history_next + CHAT_HISTORY_CAP - 1u) % CHAT_HISTORY_CAP;
        const io::u32 idx = (newest + CHAT_HISTORY_CAP - static_cast<io::u32>(chat_history_nav)) % CHAT_HISTORY_CAP;
        SetChatInput(io::char_view{ chat_history_utf8[idx], chat_history_len[idx] });
        FocusTextField(CHAT_TEXT_FIELD_ID);
    }

    inline void PushChatLine(const ge::net::ChatLine& line) noexcept {
        if (!chat_log) return;
        chat_log_lock.lock();
        HudChatLine& dst = chat_log[chat_log_head];
        dst.kind = line.kind;
        dst.received_ms32 = static_cast<io::u32>(io::monotonic_ms());
        dst.name_len = line.name_len;
        if (dst.name_len > ge::net::CHAT_NAME_MAX) dst.name_len = static_cast<io::u8>(ge::net::CHAT_NAME_MAX);
        dst.text_len = line.text_len;
        if (dst.text_len > ge::net::CHAT_TEXT_MAX) dst.text_len = static_cast<io::u8>(ge::net::CHAT_TEXT_MAX);
        for (io::u32 i = 0; i < ge::net::CHAT_NAME_MAX; ++i)
            dst.name[i] = (i < dst.name_len) ? line.name[i] : '\0';
        for (io::u32 i = 0; i < ge::net::CHAT_TEXT_MAX; ++i)
            dst.text[i] = (i < dst.text_len) ? line.text[i] : '\0';
        dst.name[dst.name_len] = '\0';
        dst.text[dst.text_len] = '\0';

        chat_log_head = (chat_log_head + 1u) % CHAT_LOG_CAP;
        if (chat_log_count < CHAT_LOG_CAP) ++chat_log_count;
        chat_log_lock.unlock();
    }

    IO_NODISCARD inline float ChatLineAlpha(const HudChatLine& line, bool expanded, io::u32 now_ms32) const noexcept {
        if (expanded) return 1.f;
        const io::u32 age_ms = now_ms32 - line.received_ms32;
        if (age_ms <= CHAT_LINE_HOLD_MS) return 1.f;
        const io::u32 fade_age_ms = age_ms - CHAT_LINE_HOLD_MS;
        if (fade_age_ms >= CHAT_LINE_FADE_MS) return 0.f;
        const io::i32 remain_ms = static_cast<io::i32>(CHAT_LINE_FADE_MS - fade_age_ms);
        float a = static_cast<float>(remain_ms) / static_cast<float>(CHAT_LINE_FADE_MS);
        if (a < 0.f) a = 0.f;
        if (a > 1.f) a = 1.f;
        return a;
    }

    inline void PushSystemChat(io::char_view text) noexcept {
        ge::net::ChatLine line{};
        line.kind = ge::net::CHAT_KIND_SERVER;
        const io::char_view k_server_name = "SERVER";
        line.name_len = static_cast<io::u8>(k_server_name.size());
        for (io::u32 i = 0; i < line.name_len; ++i)
            line.name[i] = k_server_name[i];
        const io::usize max_n = (text.size() > ge::net::CHAT_TEXT_MAX) ? ge::net::CHAT_TEXT_MAX : text.size();
        line.text_len = static_cast<io::u8>(max_n);
        for (io::u32 i = 0; i < line.text_len; ++i)
            line.text[i] = text[i];
        PushChatLine(line);
    }

    inline void ResetChatLog() noexcept {
        chat_log_lock.lock();
        chat_log_head = 0u;
        chat_log_count = 0u;
        if (chat_log)
            for (io::u32 i = 0; i < CHAT_LOG_CAP; ++i)
                chat_log[i] = {};
        chat_log_lock.unlock();
        ClearChatInput();
        chat_open = false;
        chat_restore_cursor_visible = false;
    }

    inline void OpenHelpWindow() noexcept {
        if (screen != ScreenState::InGame) return;
        if (inventory_open) CloseInventoryWindow();
        help_window_open = true;
        help_window_page = 0u;
        help_window_restore_cursor_visible = isCursorVisible();
        setCursorVisible(true);
        frame.first_mouse_sample = true;
    }

    inline void CloseHelpWindow() noexcept {
        if (!help_window_open) return;
        help_window_open = false;
        help_window_page = 0u;
        if (!chat_open && !inventory_open) {
            setCursorVisible(help_window_restore_cursor_visible);
            frame.first_mouse_sample = true;
        }
    }

    inline void OpenChatInput(io::char_view preset = {}) noexcept {
        if (screen != ScreenState::InGame || chat_open) return;
        if (help_window_open) CloseHelpWindow();
        if (inventory_open) CloseInventoryWindow();
        chat_open = true;
        chat_history_nav = -1;
        chat_history_draft_len = 0u;
        chat_history_draft_utf8[0] = '\0';
        chat_restore_cursor_visible = isCursorVisible();
        setCursorVisible(true);
        frame.first_mouse_sample = true;
        FocusTextField(CHAT_TEXT_FIELD_ID);
        if (!preset.empty()) SetChatInput(preset);
        else UpdateChatSuggestions();
    }

    inline void CloseChatInput(bool suppress_return_reopen = false) noexcept {
        if (!chat_open) return;
        chat_open = false;
        if (suppress_return_reopen)
            chat_suppress_return_reopen = true;
        setCursorVisible(chat_restore_cursor_visible);
        frame.first_mouse_sample = true;
        chat_history_nav = -1;
        chat_history_draft_len = 0u;
        chat_history_draft_utf8[0] = '\0';
        ClearChatInput();
        ResetChatSuggestions();
        ClearFocusedTextField(CHAT_TEXT_FIELD_ID);
    }

    inline void ClearInventoryPick() noexcept {
        inventory_hover_valid = false;
        inventory_hover_region = ge::item::SlotRegion::Hotbar;
        inventory_hover_index = 0u;
        ward_config_hover_valid = false;
        ward_config_hover_slot = 0u;
    }

    inline void OpenInventoryWindow() noexcept {
        if (screen != ScreenState::InGame || inventory_open) return;
        if (help_window_open) CloseHelpWindow();
        if (chat_open) CloseChatInput();
        inventory_open = true;
        inventory_restore_cursor_visible = isCursorVisible();
        setCursorVisible(true);
        frame.first_mouse_sample = true;
        ClearInventoryPick();
    }

    inline void CloseInventoryWindow() noexcept {
        if (!inventory_open) return;
        inventory_open = false;
        setCursorVisible(inventory_restore_cursor_visible);
        frame.first_mouse_sample = true;
        ClearInventoryPick();
    }

    inline void ToggleInventoryWindow() noexcept {
        if (inventory_open) CloseInventoryWindow();
        else OpenInventoryWindow();
    }

    inline void QueueUseSelectedItem() noexcept {
        ge::net::InventoryAction action{};
        action.action = ge::net::INVENTORY_ACTION_USE_SELECTED;
        (void)EnqueueNetInventoryAction(action);
    }

    inline void QueueMeleeAttack() noexcept {
        if (screen != ScreenState::InGame) return;
        if (chat_open || inventory_open || player_dead) return;
        const io::u64 now_ms = io::monotonic_ms();
        if (now_ms - net_last_melee_sent_ms < 120u)
            return;
        net_last_melee_sent_ms = now_ms;
        (void)EnqueueNetMeleeAttack(camera.yaw, camera.pitch);
    }

    IO_NODISCARD inline bool InventoryCursorActive() const noexcept {
        return !ge::item::is_empty(inventory_state.cursor);
    }

    IO_NODISCARD static inline bool IsWardStack(const ge::item::Stack& stack) noexcept {
        if (ge::item::is_empty(stack)) return false;
        return ge::item::def(stack.id).category == ge::item::Category::SpellingWards;
    }

    IO_NODISCARD static inline bool IsSpellStack(const ge::item::Stack& stack) noexcept {
        if (ge::item::is_empty(stack)) return false;
        return ge::item::def(stack.id).category == ge::item::Category::Spells;
    }

    IO_NODISCARD static inline io::u16 WardTokenFromStack(const ge::item::Stack& stack) noexcept {
        if (!IsWardStack(stack)) return 0u;
        return stack.freshness;
    }

    IO_NODISCARD inline bool HasWardAtIndex(io::u32 index) const noexcept {
        if (index >= ge::item::INVENTORY_SLOT_COUNT) return false;
        return WardTokenFromStack(inventory_state.spelling_wards[index]) != 0u;
    }

    IO_NODISCARD inline WardConfigState* FindWardConfigByToken(io::u16 token) noexcept {
        if (token == 0u) return nullptr;
        for (io::u32 i = 0u; i < WARD_CONFIG_CACHE_CAP; ++i) {
            WardConfigState& cfg = ward_configs[i];
            if (!cfg.valid) continue;
            if (cfg.ward_instance == token) return &cfg;
        }
        return nullptr;
    }

    IO_NODISCARD inline const WardConfigState* FindWardConfigByToken(io::u16 token) const noexcept {
        return const_cast<Window*>(this)->FindWardConfigByToken(token);
    }

    IO_NODISCARD inline io::u16 SelectedWardToken() const noexcept {
        if (ward_config_selected_index >= ge::item::INVENTORY_SLOT_COUNT)
            return 0u;
        return WardTokenFromStack(inventory_state.spelling_wards[ward_config_selected_index]);
    }

    IO_NODISCARD inline WardConfigState* SelectedWardConfig() noexcept {
        return FindWardConfigByToken(SelectedWardToken());
    }

    IO_NODISCARD inline const WardConfigState* SelectedWardConfig() const noexcept {
        return FindWardConfigByToken(SelectedWardToken());
    }

    IO_NODISCARD static inline bool TryResolveWardSlotFromRegion(ge::item::SlotRegion region,
                                                                 io::u8 index,
                                                                 io::u8& out_ward_slot) noexcept {
        out_ward_slot = 0u;
        if (region == ge::item::SlotRegion::SpellingWards) {
            out_ward_slot = index;
            return index < ge::item::INVENTORY_SLOT_COUNT;
        }
        if (region != ge::item::SlotRegion::General)
            return false;
        static constexpr io::u8 GENERAL_THIRD = static_cast<io::u8>(ge::item::INVENTORY_SLOT_COUNT / 3u);
        static constexpr io::u8 WARD_BASE = static_cast<io::u8>(GENERAL_THIRD * 2u);
        if (index < WARD_BASE || index >= static_cast<io::u8>(WARD_BASE + GENERAL_THIRD))
            return false;
        out_ward_slot = static_cast<io::u8>(index - WARD_BASE);
        return true;
    }

    inline void ResetWardConfigs() noexcept {
        for (io::u32 i = 0u; i < WARD_CONFIG_CACHE_CAP; ++i)
            ward_configs[i] = {};
        ward_config_open = false;
        ward_config_selected_index = 0u;
        ward_config_hover_valid = false;
        ward_config_hover_slot = 0u;
    }

    inline void ApplyIncomingWardConfigState(const ge::net::WardConfigStateSample& state) noexcept {
        if (!state.valid || state.ward_instance == 0u)
            return;
        WardConfigState* cfg = FindWardConfigByToken(state.ward_instance);
        if (!cfg) {
            for (io::u32 i = 0u; i < WARD_CONFIG_CACHE_CAP; ++i) {
                if (ward_configs[i].valid) continue;
                cfg = &ward_configs[i];
                break;
            }
            if (!cfg)
                cfg = &ward_configs[0];
        }
        cfg->valid = true;
        cfg->ward_instance = state.ward_instance;
        cfg->slots_available = state.slots_available;
        cfg->stat_speed = state.stat_speed;
        cfg->stat_delay_cast = state.stat_delay_cast;
        cfg->stat_delay_reload = state.stat_delay_reload;
        cfg->stat_spread = state.stat_spread;
        cfg->snapshot_ms = io::monotonic_ms();
        for (io::u32 i = 0u; i < WARD_CONFIG_SLOT_MAX; ++i)
            cfg->spells[i] = state.spells[i];
    }

    inline void SyncWardConfigsFromInventory() noexcept {
        for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) {
            if (HasWardAtIndex(i))
                continue;
            if (ward_config_selected_index == i)
                ward_config_open = false;
        }

        if (ward_config_selected_index >= ge::item::INVENTORY_SLOT_COUNT || !HasWardAtIndex(ward_config_selected_index)) {
            io::u8 next_index = 0u;
            bool found = false;
            for (io::u32 i = 0u; i < ge::item::INVENTORY_SLOT_COUNT; ++i) {
                if (!HasWardAtIndex(i)) continue;
                next_index = static_cast<io::u8>(i);
                found = true;
                break;
            }
            ward_config_selected_index = next_index;
            if (!found)
                ward_config_open = false;
        }
    }

    inline void QueueWardConfigClick(io::u8 slot_index, bool right_click) noexcept {
        if (!ward_config_open) return;
        if (slot_index >= WARD_CONFIG_SLOT_MAX) return;
        const io::u16 token = SelectedWardToken();
        if (token == 0u) return;
        WardConfigState* cfg = SelectedWardConfig();
        if (!cfg || !cfg->valid) return;
        if (slot_index >= cfg->slots_available) return;

        ge::net::WardConfigActionSample action{};
        action.ward_instance = token;
        action.ward_slot = slot_index;
        action.right_click = right_click;
        (void)EnqueueNetWardConfigAction(action);
    }

    IO_NODISCARD static inline io::u32 DecayRemainingMsFromFreshness(io::u16 freshness, io::u32 decay_ms) noexcept {
        const io::u32 fresh_u32 = static_cast<io::u32>(freshness);
        if (fresh_u32 == 0u || decay_ms == 0u)
            return 0u;
        if (fresh_u32 >= ge::item::FRESHNESS_MAX)
            return decay_ms;
        if (decay_ms <= (0xFFFFFFFFu / ge::item::FRESHNESS_MAX))
            return (fresh_u32 * decay_ms) / ge::item::FRESHNESS_MAX;
        const io::u32 whole = decay_ms / ge::item::FRESHNESS_MAX;
        const io::u32 rem = decay_ms % ge::item::FRESHNESS_MAX;
        return fresh_u32 * whole + (fresh_u32 * rem) / ge::item::FRESHNESS_MAX;
    }

    IO_NODISCARD inline io::u32 EstimateStackDecayRemainingMs(const ge::item::Stack& stack) const noexcept {
        if (!ge::item::decays(stack.id))
            return 0u;
        const io::u32 decay_ms = ge::item::def(stack.id).decay_ms;
        io::u32 remaining_ms = DecayRemainingMsFromFreshness(stack.freshness, decay_ms);
        const io::u64 now_ms = io::monotonic_ms();
        io::u32 elapsed_ms = 0u;
        if (now_ms > inventory_last_snapshot_ms) {
            const io::u64 delta = now_ms - inventory_last_snapshot_ms;
            elapsed_ms = (delta > 0xFFFFFFFFull) ? 0xFFFFFFFFu : static_cast<io::u32>(delta);
        }
        if (elapsed_ms >= remaining_ms)
            return 0u;
        return remaining_ms - elapsed_ms;
    }

    IO_NODISCARD inline const ge::item::Stack* InventoryTooltipStack() const noexcept {
        if (!inventory_open)
            return nullptr;
        if (InventoryCursorActive())
            return &inventory_state.cursor;
        if (ward_config_open && ward_config_hover_valid &&
            ward_config_selected_index < ge::item::INVENTORY_SLOT_COUNT &&
            HasWardAtIndex(ward_config_selected_index)) {
            const WardConfigState* cfg = SelectedWardConfig();
            if (cfg && cfg->valid &&
                ward_config_hover_slot < cfg->slots_available &&
                ward_config_hover_slot < WARD_CONFIG_SLOT_MAX &&
                !ge::item::is_empty(cfg->spells[ward_config_hover_slot])) {
                return &cfg->spells[ward_config_hover_slot];
            }
        }
        if (!inventory_hover_valid)
            return nullptr;
        const ge::item::Stack* stack = ge::item::slot_ptr(inventory_state, inventory_hover_region, inventory_hover_index);
        if (!stack || ge::item::is_empty(*stack))
            return nullptr;
        return stack;
    }

    inline void QueueMoveInventorySlot(ge::item::SlotRegion src_region, io::u8 src_index,
                                       ge::item::SlotRegion dst_region, io::u8 dst_index) noexcept {
        ge::net::InventoryAction action{};
        action.action = ge::net::INVENTORY_ACTION_MOVE;
        action.src_region = src_region;
        action.src_index = src_index;
        action.dst_region = dst_region;
        action.dst_index = dst_index;
        if (EnqueueNetInventoryAction(action))
            (void)ge::item::move_between_player_slots(inventory_state, src_region, src_index, dst_region, dst_index);
    }

    inline void QueueInventoryClick(ge::item::SlotRegion region, io::u8 index, bool right_click) noexcept {
        ge::net::InventoryAction action{};
        action.action = right_click ? ge::net::INVENTORY_ACTION_RIGHT_CLICK : ge::net::INVENTORY_ACTION_LEFT_CLICK;
        action.src_region = region;
        action.src_index = index;
        if (!EnqueueNetInventoryAction(action))
            return;
        if (right_click)
            (void)ge::item::right_click(inventory_state, region, index);
        else
            (void)ge::item::left_click(inventory_state, region, index);
    }

    inline void QueueInventoryDropSlot(ge::item::SlotRegion region, io::u8 index, bool one_only) noexcept {
        ge::net::InventoryAction action{};
        action.action = one_only ? ge::net::INVENTORY_ACTION_DROP_SLOT_ONE : ge::net::INVENTORY_ACTION_DROP_SLOT_STACK;
        action.src_region = region;
        action.src_index = index;
        if (!EnqueueNetInventoryAction(action))
            return;
        ge::item::Stack dropped{};
        (void)ge::item::drop_from_slot(inventory_state, region, index, one_only ? 1u : 0u, dropped);
    }

    inline void QueueInventoryDropCursor(bool one_only) noexcept {
        ge::net::InventoryAction action{};
        action.action = one_only ? ge::net::INVENTORY_ACTION_DROP_CURSOR_ONE : ge::net::INVENTORY_ACTION_DROP_CURSOR_STACK;
        action.src_region = ge::item::SlotRegion::Cursor;
        if (!EnqueueNetInventoryAction(action))
            return;
        ge::item::Stack dropped{};
        (void)ge::item::drop_from_slot(inventory_state, ge::item::SlotRegion::Cursor, 0u, one_only ? 1u : 0u, dropped);
    }

    inline void ResetMovementModes() noexcept {
        use_fly = false;
        use_noclip = false;
        player_sneaking = false;
        player_crawling = false;
        player_crawl_toggle = false;
        player_crawl_prev_grounded = false;
        if (player_ecs) {
            player_ecs->flags[0].use_fly = false;
            player_ecs->flags[0].use_noclip = false;
        }
    }

    inline void ResetPlayerState() noexcept {
        move_velocity = { 0.f, 0.f, 0.f };
        player_hp = 100.f;
        player_hunger = 255u;
        player_dead = false;
        player_death_reason = ge::net::DeathReason::None;
        player_grounded = false;
        player_airborne = false;
        player_sneaking = false;
        player_crawling = false;
        player_crawl_toggle = false;
        player_crawl_prev_grounded = false;
        player_sneak_view_offset = 0.f;
        player_air_peak_foot_y = 0.f;
        player_last_fall_blocks = 0.f;
        player_last_fall_damage = 0.f;
        dev_hud_visible = false;
        player_hud_visible = false;
        help_window_open = false;
        help_window_page = 0u;
        help_window_restore_cursor_visible = false;
        inventory_open = false;
        inventory_flags = 0u;
        ClearInventoryPick();
        ResetHotbarHintTracking();
        target_block_valid = false;
        ResetTargetBreak();
        ClearRemotePlayers();
        SyncLocalPlayerEcsFromRuntime();
    }

    inline void SyncLocalPlayerEcsFromRuntime() noexcept {
        if (!player_ecs) return;
        player_ecs->alive[0] = 1u;
        player_ecs->transform[0].x = camera.position[0];
        player_ecs->transform[0].y = camera.position[1];
        player_ecs->transform[0].z = camera.position[2];
        player_ecs->velocity[0].x = move_velocity[0];
        player_ecs->velocity[0].y = move_velocity[1];
        player_ecs->velocity[0].z = move_velocity[2];
        player_ecs->health[0].hp = static_cast<io::u16>(io::to_u32(player_hp + 0.5f));
        player_ecs->flags[0].use_fly = use_fly;
        player_ecs->flags[0].use_noclip = use_noclip;
        player_ecs->flags[0].grounded = player_grounded;
        player_ecs->flags[0].airborne = player_airborne;
        player_ecs->runtime[0].air_peak_foot_y = player_air_peak_foot_y;
    }

    inline void SyncRuntimeFromLocalPlayerEcs() noexcept {
        if (!player_ecs || player_ecs->alive[0] == 0u) return;
        camera.position[0] = player_ecs->transform[0].x;
        camera.position[1] = player_ecs->transform[0].y;
        camera.position[2] = player_ecs->transform[0].z;
        move_velocity[0] = player_ecs->velocity[0].x;
        move_velocity[1] = player_ecs->velocity[0].y;
        move_velocity[2] = player_ecs->velocity[0].z;
        player_hp = static_cast<float>(player_ecs->health[0].hp);
        player_dead = player_hp <= 0.f;
        if (!player_dead) player_death_reason = ge::net::DeathReason::None;
        use_fly = player_ecs->flags[0].use_fly;
        use_noclip = player_ecs->flags[0].use_noclip;
        player_grounded = player_ecs->flags[0].grounded;
        player_airborne = player_ecs->flags[0].airborne;
        player_air_peak_foot_y = player_ecs->runtime[0].air_peak_foot_y;
    }

    IO_NODISCARD inline bool SendModeCommandToServer(io::char_view text) noexcept {
        if (net_state.load() != 2u) return false;
        return EnqueueNetChatMessage(PlayerNameView(), text);
    }

    inline void SetGodModeEnabled(bool enable, bool sync_server) noexcept {
        const bool was_fly = use_fly;
        use_fly = enable;
        SyncLocalPlayerEcsFromRuntime();

        if (!sync_server || net_state.load() != 2u)
            return;

        const auto send_mode_cmd = [&](io::char_view cmd, io::char_view pending_msg) noexcept {
            if (!SendModeCommandToServer(cmd))
                PushSystemChat(pending_msg);
        };

        if (enable) {
            if (!was_fly) send_mode_cmd("/godmode", "Server sync pending: /godmode not sent");
            return;
        }
        if (was_fly) send_mode_cmd("/godmode", "Server sync pending: /godmode not sent");
    }

    IO_NODISCARD inline bool TryHandleLocalModeCommand(io::char_view trimmed) noexcept {
        if (ChatCommandEq(trimmed, "/godmode")) {
            SetGodModeEnabled(!use_fly, true);
            PushSystemChat(use_fly ? "godmode: enabled" : "godmode: disabled");
            return true;
        }

        if (ChatCommandEq(trimmed, "/noclip")) {
            use_noclip = !use_noclip;
            SyncLocalPlayerEcsFromRuntime();
            if (!SendModeCommandToServer("/noclip")) {
                PushSystemChat(use_noclip ? "noclip: enabled" : "noclip: disabled");
                PushSystemChat("Server sync pending: /noclip not sent");
            }
            return true;
        }

        return false;
    }

    inline void SubmitChatInput() noexcept {
        if (!chat_open) return;
        const io::char_view raw_trimmed = TrimChatText(io::char_view{ chat_input_utf8, chat_input_len });
        io::StackOut<256> canonical{};
        io::char_view trimmed = raw_trimmed;
        if (BuildCanonicalCommandWithCoordinates(raw_trimmed, canonical, true))
            trimmed = canonical.view();
        if (trimmed.empty()) {
            CloseChatInput(true);
            return;
        }

        PushChatHistoryEntry(trimmed);

        if (ChatCommandEq(trimmed, "/help")) {
            CloseChatInput(true);
            OpenHelpWindow();
            return;
        }

        if (TryHandleLocalModeCommand(trimmed)) {
            CloseChatInput(true);
            return;
        }

        if (net_state.load() != 2u) {
            PushSystemChat("Cannot send chat: not connected");
            CloseChatInput(true);
            return;
        }

        if (!EnqueueNetChatMessage(PlayerNameView(), trimmed))
            PushSystemChat("Cannot send chat: outbound queue is full");
        CloseChatInput(true);
    }

    inline io::u32 ClampMeshWorkers(io::u32 value) const noexcept {
        if (value < mesh_worker_min) return mesh_worker_min;
        if (value > mesh_worker_max) return mesh_worker_max;
        return value;
    }

    IO_NODISCARD static inline bool TryMulUsize(io::usize a, io::usize b, io::usize& out) noexcept {
        if (a == 0 || b == 0) {
            out = 0;
            return true;
        }
        const io::usize maxv = static_cast<io::usize>(-1);
        if (a > maxv / b) return false;
        out = a * b;
        return true;
    }

    IO_NODISCARD static inline bool ComputeChunkSlotCountFor(io::u32 rd, io::usize& out_count) noexcept {
        io::u32 ry = rd * 2u;
        if (ry < static_cast<io::u32>(WORLD_Y_RADIUS_CHUNKS))
            ry = static_cast<io::u32>(WORLD_Y_RADIUS_CHUNKS);

        const io::usize sx = static_cast<io::usize>(rd * 2u + 1u);
        const io::usize sy = static_cast<io::usize>(ry * 2u + 1u);
        const io::usize sz = sx;

        io::usize plane = 0;
        if (!TryMulUsize(sx, sz, plane)) return false;
        if (!TryMulUsize(plane, sy, out_count)) return false;
        return true;
    }

    static constexpr io::u32 MAX_RENDER_DISTANCE_CHUNKS = 32u;

    IO_NODISCARD inline io::u32 MaxSafeRenderDistance() const noexcept {
        return MAX_RENDER_DISTANCE_CHUNKS;
    }

    inline io::u32 ClampRenderDistance(io::u32 value) const noexcept {
        const io::u32 max_safe = MaxSafeRenderDistance();
        if (value < 1u) value = 1u;
        if (value > max_safe) value = max_safe;
        return value;
    }

    inline io::u32 ClampExtraRadius(io::u32 value) const noexcept {
        if (value < 1u) return 1u;
        if (value > 16u) return 16u;
        return value;
    }

    inline void DetectThreadLimits() noexcept {
        hw_threads = io::Thread::max_workers();
        if (hw_threads == 0) hw_threads = 1;
        mesh_worker_min = 1u;
        mesh_worker_max = hw_threads;
    }

    inline void LoadRuntimeConfig() noexcept {
        DetectThreadLimits();

        player_name_len = 0;
        for (io::usize i = 0; i < 32; ++i) {
            const char ch = runtime_cfg.client.player_name_utf8[i];
            if (ch == '\0') break;
            player_name_utf8[player_name_len++] = (ch == '\n' || ch == '\r') ? ' ' : ch;
        }
        ClampPlayerNameLen();
        player_name_hash = HashName(player_name_utf8, player_name_len);

        render_distance_chunks = ClampRenderDistance(runtime_cfg.client.render_distance_chunks);
        render_distance_pending = static_cast<float>(render_distance_chunks);
        extra_radius = ClampExtraRadius(runtime_cfg.client.extra_radius);
        extra_radius_pending = static_cast<float>(extra_radius);
        is_dark_theme = runtime_cfg.client.is_dark_theme;

        mesh_workers_configured = ClampMeshWorkers(runtime_cfg.client.mesh_workers);
        mesh_workers_pending = static_cast<float>(mesh_workers_configured);
    }

    inline void SaveRuntimeConfig() noexcept {
        runtime_cfg.header = {};
        for (io::usize i = 0; i < 32; ++i)
            runtime_cfg.client.player_name_utf8[i] = (i < player_name_len) ? player_name_utf8[i] : '\0';
        runtime_cfg.client.render_distance_chunks = render_distance_chunks;
        runtime_cfg.client.mesh_workers = mesh_workers_configured;
        runtime_cfg.client.extra_radius = extra_radius;
        runtime_cfg.client.is_dark_theme = is_dark_theme;
        (void)ge::save_config_binary(runtime_cfg);
    }

    inline void UpdatePlayerNameIfChanged() noexcept {
        ClampPlayerNameLen();
        for (io::usize i = 0; i < player_name_len; ++i)
            if (player_name_utf8[i] == '\n' || player_name_utf8[i] == '\r')
                player_name_utf8[i] = ' ';
        const io::u32 hash = HashName(player_name_utf8, player_name_len);
        if (hash != player_name_hash) {
            player_name_hash = hash;
            SaveRuntimeConfig();
        }
    }

    inline bool InitMeshWorkerPool(io::u32 wanted_threads) noexcept {
        if (!worker_pool) return false;
        mesh_workers_configured = ClampMeshWorkers(wanted_threads);
        mesh_workers_pending = static_cast<float>(mesh_workers_configured);
        chunk_job_slots_active = 0u;

        unsigned cap = 64u;
        const unsigned wanted = static_cast<unsigned>(mesh_workers_configured) * 8u;
        while (cap < wanted) cap <<= 1u;

        if (!worker_pool->init(static_cast<unsigned>(mesh_workers_configured), cap)) {
            mesh_worker_pool = 0;
            mesh_worker_threads = 0;
            return false;
        }
        mesh_worker_pool = mesh_workers_configured;
        mesh_worker_threads = mesh_workers_configured;

        io::u32 slots = mesh_worker_pool * 8u;
        if (slots == 0u) slots = 1u;
        if (slots > CHUNK_JOB_MAX_SLOTS) slots = CHUNK_JOB_MAX_SLOTS;
        chunk_job_slots_active = slots;
        return InitChunkJobSlots();
    }

    inline bool ReinitMeshWorkerPool(io::u32 wanted_threads) noexcept {
        WaitChunkJobsIdle();
        if (worker_pool)
            worker_pool->shutdown(true);
        return InitMeshWorkerPool(wanted_threads);
    }

    inline void ApplyRenderDistance() noexcept {
        io::u32 value = io::to_u32(static_cast<double>(render_distance_pending + 0.5f));
        value = ClampRenderDistance(value);
        render_distance_chunks = value;
        render_distance_pending = static_cast<float>(value);
    }

    inline void ApplyExtraRadius() noexcept {
        io::u32 value = io::to_u32(static_cast<double>(extra_radius_pending + 0.5f));
        value = ClampExtraRadius(value);
        extra_radius = value;
        extra_radius_pending = static_cast<float>(value);
    }

    inline void ApplyMeshWorkers() noexcept {
        io::u32 value = io::to_u32(static_cast<double>(mesh_workers_pending + 0.5f));
        value = ClampMeshWorkers(value);
        mesh_workers_pending = static_cast<float>(value);

        if (value != mesh_workers_configured)
            (void)ReinitMeshWorkerPool(value);
        mesh_workers_configured = value;
        if (mesh_worker_pool == 0) mesh_worker_threads = 0;
        else mesh_worker_threads = mesh_worker_pool;
    }

    inline void ApplyGraphicsSettings() noexcept {
        ApplyRenderDistance();
        ApplyExtraRadius();
        ApplyMeshWorkers();
        if (!InitChunkJobSlots()) {
#ifdef _DEBUG
            io::out << "[chunk] failed to init job slots\n";
#endif
        }
        if (screen == ScreenState::InGame) {
            if (!InitChunkWorld()) {
#ifdef _DEBUG
                io::out << "[chunk] failed to rebuild chunk world after graphics apply\n";
#endif
            }
        }
        SaveRuntimeConfig();
    }

    static inline bool IsKeyNone(hi::Key key) noexcept {
        return key == hi::Key::__NONE__;
    }

    inline ge::KeyBinding& Binding(Action action) noexcept {
        return key_bindings[static_cast<io::usize>(action)];
    }

    inline const ge::KeyBinding& Binding(Action action) const noexcept {
        return key_bindings[static_cast<io::usize>(action)];
    }

    inline bool IsActionPressed(Action action) const noexcept {
        const ge::KeyBinding& b = Binding(action);
        if (IsKeyNone(b.key)) return false;
        if (IsKeyNone(b.maybe_second_key)) return hi::Key_t::isPressed(b.key);
        return hi::Key_t::isPressed(b.key) && hi::Key_t::isPressed(b.maybe_second_key);
    }

    inline bool IsActionTriggeredOnKeyUp(Action action, hi::Key released) const noexcept {
        const ge::KeyBinding& b = Binding(action);
        if (IsKeyNone(b.key)) return false;
        if (IsKeyNone(b.maybe_second_key)) return released == b.key;
        if (released == b.key) return hi::Key_t::isPressed(b.maybe_second_key);
        if (released == b.maybe_second_key) return hi::Key_t::isPressed(b.key);
        return false;
    }

    inline bool SaveKeyBindings() noexcept {
        return ge::save_key_bindings_binary(io::view<const ge::KeyBinding>{ key_bindings, ACTION_COUNT });
    }

    inline bool LoadKeyBindings() noexcept {
        return ge::ensure_key_bindings_binary(
            io::view<ge::KeyBinding>{ key_bindings, ACTION_COUNT },
            io::view<const ge::KeyBinding>{ DEFAULT_KEY_BINDINGS, ACTION_COUNT },
            key_bindings_meta);
    }

    template<io::usize N>
    static inline void AppendBindingText(io::StackOut<N>& out, const ge::KeyBinding& binding) noexcept {
        if (binding.key == hi::Key::__NONE__) {
            out << "<none>";
            return;
        }
        out << hi::Key_t::map(binding.key);
        if (binding.maybe_second_key != hi::Key::__NONE__)
            out << " + " << hi::Key_t::map(binding.maybe_second_key);
    }

    inline void CancelRebind() noexcept {
        setting_rebind = false;
        rebind_action_index = 0;
        rebind_step = 0;
        pending_rebind = {};
    }

    inline void StartRebind(Action action) noexcept {
        setting_rebind = true;
        rebind_action_index = static_cast<io::usize>(action);
        rebind_step = 1;
        pending_rebind = {};
    }

    inline void CommitPendingRebind() noexcept {
        if (!setting_rebind || rebind_action_index >= ACTION_COUNT) {
            CancelRebind();
            return;
        }
        if (pending_rebind.key == hi::Key::__NONE__) {
            pending_rebind = key_bindings[rebind_action_index];
        }
        key_bindings[rebind_action_index] = pending_rebind;
        SaveKeyBindings();
        CancelRebind();
    }

    inline void ResetBinding(Action action) noexcept {
        const io::usize index = static_cast<io::usize>(action);
        if (index >= ACTION_COUNT) return;
        key_bindings[index] = DEFAULT_KEY_BINDINGS[index];
        if (setting_rebind && rebind_action_index == index)
            CancelRebind();
        SaveKeyBindings();
    }

    inline void ResetAllBindings() noexcept {
        for (io::usize i = 0; i < ACTION_COUNT; ++i)
            key_bindings[i] = DEFAULT_KEY_BINDINGS[i];
        CancelRebind();
        SaveKeyBindings();
    }

    inline bool HandleRebindKeyDown(hi::Key key) noexcept {
        if (!setting_rebind) return false;

        if (key == hi::Key::Escape) {
            CancelRebind();
            return true;
        }

        if (rebind_step == 1) {
            pending_rebind.key = key;
            pending_rebind.maybe_second_key = hi::Key::__NONE__;
            rebind_step = 2;
            return true;
        }

        if (key == hi::Key::Backspace || key == hi::Key::Delete || key == hi::Key::Return || key == pending_rebind.key) {
            pending_rebind.maybe_second_key = hi::Key::__NONE__;
            CommitPendingRebind();
            return true;
        }

        pending_rebind.maybe_second_key = key;
        CommitPendingRebind();
        return true;
    }

#include "../state/state_session_actor.hpp"
#include "../state/state_storage_queue.hpp"
#include "../state/state_network.hpp"
#include "window_world.hpp"
#include "ui_common.hpp"
#include "ui_contexts.hpp"
#include "ui_resources.hpp"
};

namespace ge {
namespace client {
namespace render {
    inline void ScenePipeline::RenderFrame(Window& win, float dt) noexcept {
        if (!win.frame.game_window_focused)
            io::sleep_ms(20);

        win.frame.scene_time += dt;
        win.frame.last_dt = dt;

        win.ScenePipelineUpdateCamera(dt);
        win.ScenePipelineRenderWorld();

        gl::Disable(gl::Capability::DepthTest);
        UiPipeline::Render(win, dt);
        gl::Enable(gl::Capability::DepthTest);
    }

    inline void UiPipeline::Render(Window& win, float dt) noexcept {
        switch (win.CurrentScreen()) {
        case ScreenState::MainMenu:    win.UiRenderMainMenu(dt); break;
        case ScreenState::Settings:    win.UiRenderSettings(); break;
        case ScreenState::Graphics:    win.UiRenderGraphics(); break;
        case ScreenState::KeyBindings: win.UiRenderKeyBindings(); break;
        case ScreenState::Multiplayer: win.UiRenderMultiplayer(); break;
        case ScreenState::Connecting:  win.UiRenderConnecting(); break;
        case ScreenState::InGame:      win.UiRenderInGame(dt); break;
        case ScreenState::InGameDead:  win.UiRenderInGameDead(dt); break;
        }
        win.UiPipelineFlushText();
    }
} // namespace render
} // namespace client
} // namespace ge


