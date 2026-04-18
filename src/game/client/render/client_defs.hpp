#pragma once

#include "hi/hi/hi.hpp"

#include "../../../engine/core/key_bindings.hpp"
#include "../../../engine/core/camera.hpp"
#include "../../../engine/core/dt_history.hpp"
#include "../../../engine/voxel/world.hpp"
#include "../../../engine/voxel/mesh_builder.hpp"

enum class ScreenState : io::u8 {
    MainMenu = 0,
    Settings,
    Graphics,
    KeyBindings,
    Multiplayer,
    Connecting,
    InGame,
    InGameDead
};

enum class SessionMode : io::u8 {
    None = 0,
    Singleplayer,
    Multiplayer
};

struct GameSessionContext {
    SessionMode mode = SessionMode::None;
    char endpoint_utf8[64]{};
    io::usize endpoint_len = 0;
    char server_name_utf8[32]{};
    io::usize server_name_len = 0;
};

// Input/action map for client gameplay + UI.
enum class Action : io::u8 {
    MoveForward = 0,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    ToggleCursor,
    ToggleFullscreen,
    ToggleWireframe,
    ToggleVSync,
    BackToMenu,
    Sprint,
    QuickSlot1,
    QuickSlot2,
    QuickSlot3,
    QuickSlot4,
    QuickSlot5,
    QuickSlot6,
    QuickSlot7,
    QuickSlot8,
    QuickSlot9,
    ToggleInventory,
    ChatSuggestionPrev,
    ChatSuggestionNext,
    DisableInGameHud,
    Sneak,
    Crawl,
    __COUNT__
};

static constexpr io::usize ACTION_COUNT = static_cast<io::usize>(Action::__COUNT__);
static constexpr ge::KeyBinding DEFAULT_KEY_BINDINGS[ACTION_COUNT]{
    { hi::Key::W, hi::Key::__NONE__ },        // MoveForward
    { hi::Key::S, hi::Key::__NONE__ },        // MoveBackward
    { hi::Key::A, hi::Key::__NONE__ },        // MoveLeft
    { hi::Key::D, hi::Key::__NONE__ },        // MoveRight
    { hi::Key::E, hi::Key::__NONE__ },        // MoveUp
    { hi::Key::Q, hi::Key::__NONE__ },        // MoveDown
    { hi::Key::F1, hi::Key::__NONE__ },       // ToggleCursor
    { hi::Key::F11, hi::Key::__NONE__ },      // ToggleFullscreen
    { hi::Key::F9, hi::Key::__NONE__ },       // ToggleWireframe
    { hi::Key::F12, hi::Key::__NONE__ },      // ToggleVSync
    { hi::Key::Escape, hi::Key::__NONE__ },   // BackToMenu
    { hi::Key::Control, hi::Key::__NONE__ },  // Sprint
    { hi::Key::_1, hi::Key::__NONE__ },       // QuickSlot1
    { hi::Key::_2, hi::Key::__NONE__ },       // QuickSlot2
    { hi::Key::_3, hi::Key::__NONE__ },       // QuickSlot3
    { hi::Key::_4, hi::Key::__NONE__ },       // QuickSlot4
    { hi::Key::_5, hi::Key::__NONE__ },       // QuickSlot5
    { hi::Key::_6, hi::Key::__NONE__ },       // QuickSlot6
    { hi::Key::_7, hi::Key::__NONE__ },       // QuickSlot7
    { hi::Key::_8, hi::Key::__NONE__ },       // QuickSlot8
    { hi::Key::_9, hi::Key::__NONE__ },       // QuickSlot9
    { hi::Key::I, hi::Key::__NONE__ },        // ToggleInventory
    { hi::Key::Left, hi::Key::__NONE__ },     // ChatSuggestionPrev
    { hi::Key::Right, hi::Key::__NONE__ },    // ChatSuggestionNext
    { hi::Key::F2, hi::Key::__NONE__ },       // DisableInGameHud
    { hi::Key::Shift, hi::Key::__NONE__ },    // Sneak
    { hi::Key::Z, hi::Key::__NONE__ },        // Crawl
};

static inline io::char_view ActionName(Action action) noexcept {
    switch (action) {
    case Action::MoveForward: return "Move forward";
    case Action::MoveBackward: return "Move backward";
    case Action::MoveLeft: return "Move left";
    case Action::MoveRight: return "Move right";
    case Action::MoveUp: return "Move up";
    case Action::MoveDown: return "Move down";
    case Action::Sneak: return "Sneak";
    case Action::Crawl: return "Crawl";
    case Action::DisableInGameHud: return "Toggle HUD";
    case Action::ToggleCursor: return "Toggle cursor lock";
    case Action::ToggleFullscreen: return "Toggle fullscreen";
    case Action::ToggleWireframe: return "Toggle wireframe";
    case Action::ToggleVSync: return "Toggle VSync";
    case Action::BackToMenu: return "Back/Cancel";
    case Action::Sprint: return "Sprint";
    case Action::QuickSlot1: return "Quick slot 1";
    case Action::QuickSlot2: return "Quick slot 2";
    case Action::QuickSlot3: return "Quick slot 3";
    case Action::QuickSlot4: return "Quick slot 4";
    case Action::QuickSlot5: return "Quick slot 5";
    case Action::QuickSlot6: return "Quick slot 6";
    case Action::QuickSlot7: return "Quick slot 7";
    case Action::QuickSlot8: return "Quick slot 8";
    case Action::QuickSlot9: return "Quick slot 9";
    case Action::ToggleInventory: return "Toggle inventory";
    case Action::ChatSuggestionPrev: return "Chat suggestion prev";
    case Action::ChatSuggestionNext: return "Chat suggestion next";
    default: return "Unknown action";
    }
}

// GPU uniform blocks by pipeline.
struct TerrainUniforms {
    int u_model = -1;
    int u_view = -1;
    int u_proj = -1;
    int u_light_x = -1;
    int u_light_y = -1;
    int u_light_z = -1;
    int u_view_x = -1;
    int u_view_y = -1;
    int u_view_z = -1;
    int u_atlas = -1;
    int u_chunk_tiled_mode = -1;
    int u_atlas_texel = -1;
    int u_sun_dir_x = -1;
    int u_sun_dir_y = -1;
    int u_sun_dir_z = -1;
    int u_daylight = -1;
    int u_fog_r = -1;
    int u_fog_g = -1;
    int u_fog_b = -1;
    int u_fog_start = -1;
    int u_fog_end = -1;
};

struct LiquidUniforms {
    int u_model = -1;
    int u_view = -1;
    int u_proj = -1;
    int u_view_x = -1;
    int u_view_y = -1;
    int u_view_z = -1;
    int u_atlas = -1;
    int u_atlas_texel = -1;
    int u_sun_dir_x = -1;
    int u_sun_dir_y = -1;
    int u_sun_dir_z = -1;
    int u_daylight = -1;
    int u_fog_r = -1;
    int u_fog_g = -1;
    int u_fog_b = -1;
    int u_fog_start = -1;
    int u_fog_end = -1;
    int u_base_alpha = -1;
    int u_fresnel_power = -1;
    int u_fresnel_strength = -1;
    int u_edge_softness = -1;
    int u_edge_strength = -1;
    int u_oit_pass = -1;
};

struct LiquidCompositeUniforms {
    int u_scene = -1;
    int u_accum = -1;
    int u_reveal = -1;
    int u_depth = -1;
    int u_single_alpha = -1;
    int u_near_plane = -1;
    int u_far_plane = -1;
};

struct EntityUniforms {
    int u_model = -1;
    int u_view = -1;
    int u_proj = -1;
    int u_light_x = -1;
    int u_light_y = -1;
    int u_light_z = -1;
    int u_view_x = -1;
    int u_view_y = -1;
    int u_view_z = -1;
    int u_atlas = -1;
    int u_bones0 = -1;
    int u_daylight = -1;
    int u_fog_r = -1;
    int u_fog_g = -1;
    int u_fog_b = -1;
};

struct SkyUniforms {
    int u_screen = -1;
    int u_cam_forward_x = -1;
    int u_cam_forward_y = -1;
    int u_cam_forward_z = -1;
    int u_cam_right_x = -1;
    int u_cam_right_y = -1;
    int u_cam_right_z = -1;
    int u_cam_up_x = -1;
    int u_cam_up_y = -1;
    int u_cam_up_z = -1;
    int u_aspect = -1;
    int u_tan_half_fov = -1;
    int u_time_sec = -1;
    int u_day_length_sec = -1;
    int u_daylight = -1;
    int u_eclipse = -1;
    int u_region_mana = -1;
    int u_region_instability = -1;
    int u_region_decay = -1;
};

struct PostEffectUniforms {
    int u_black_strength = -1;
    int u_red_strength = -1;
    int u_dead_strength = -1;
    int u_map_tint_rg = -1;
    int u_map_tint_b = -1;
    int u_map_tint_strength = -1;
    int u_region_decay = -1;
    int u_region_instability = -1;
};

// Per-frame client runtime flags and timing.
struct ClientFrameState {
    ge::DtHistory dt_history{};
    bool wireframe_mode = false;
    bool first_mouse_sample = true;
    bool request_quit = false;
    bool game_window_focused = true;
    float scene_time = 0.f;
    float last_dt = 0.f;
};

// Chunk render/cache runtime data.
struct BlockFaceUv {
    float u0 = 0.f;
    float v0 = 0.f;
    float u1 = 1.f;
    float v1 = 1.f;
    bool valid = false;
};

struct ChunkRenderMesh {
    ge::voxel::ChunkCoord coord{};
    gl::VertexArray vao{};
    gl::Buffer vbo{ gl::BufferTarget::ArrayBuffer };
    gl::Buffer ebo{ gl::BufferTarget::ElementArrayBuffer };
    io::u32 index_count = 0;
    io::u32 built_version = 0;
    io::u8 render_mask = 0u;
    bool queued = false;
    bool uploaded = false;

    ChunkRenderMesh() noexcept = default;
    ChunkRenderMesh(const ChunkRenderMesh&) = delete;
    ChunkRenderMesh& operator=(const ChunkRenderMesh&) = delete;
    ChunkRenderMesh(ChunkRenderMesh&&) noexcept = default;
    ChunkRenderMesh& operator=(ChunkRenderMesh&&) noexcept = default;
};

struct TransparentChunkDrawItem {
    io::usize chunk_index = io::npos;
    float distance2 = 0.f;
};

enum : io::u8 {
    CHUNK_RENDER_MASK_SOLID = 1u << 0,
    CHUNK_RENDER_MASK_LIQUID = 1u << 1
};

enum class ChunkJobState : io::u32 {
    Free = 0,
    Queued = 1,
    Building = 2,
    Done = 3
};

struct ChunkMeshJobSlot {
    io::atomic<io::u32> state{ static_cast<io::u32>(ChunkJobState::Free) };
    io::usize world_chunk_index = 0;
    io::u32 chunk_version = 0;
    io::u32 vertex_count = 0;
    io::u32 index_count = 0;
    io::u32 failed = 0;
    io::u8 render_mask = 0u;
    ge::voxel::MeshBuildStats stats{};
    io::vector<ge::voxel::MeshVertex> vertices{};
    io::vector<io::u32> indices{};
    io::u32 rows_by_block_scratch[ge::voxel::BLOCK_COUNT][ge::voxel::CHUNK_SIZE]{};
    ge::voxel::GreedyRect rects_scratch[ge::voxel::CHUNK_SIZE * ge::voxel::CHUNK_SIZE]{};
};

static constexpr io::u32 CHUNK_JOB_MAX_SLOTS = 64u;
    static constexpr io::u32 CHUNK_JOB_VERTEX_CAP = 131072u;
static constexpr io::u32 CHUNK_JOB_INDEX_CAP = CHUNK_JOB_VERTEX_CAP * 3u / 2u;
static constexpr io::i32 WORLD_Y_RADIUS_CHUNKS = 2;
static constexpr io::u32 FACE_INDEX_COUNT = 6u;

struct ChunkMeshTaskArg {
    void* owner = nullptr;
    io::u32 slot_index = 0;
};

struct SceneCube {
    float x, y, z;
    float rx_speed, ry_speed, rz_speed;
};

static constexpr float cube_vertices[] = {
    // front
    -0.5f, -0.5f,  0.5f,   0.f,  0.f,  1.f,
     0.5f, -0.5f,  0.5f,   0.f,  0.f,  1.f,
     0.5f,  0.5f,  0.5f,   0.f,  0.f,  1.f,
    -0.5f,  0.5f,  0.5f,   0.f,  0.f,  1.f,
    // back
    -0.5f, -0.5f, -0.5f,   0.f,  0.f, -1.f,
     0.5f, -0.5f, -0.5f,   0.f,  0.f, -1.f,
     0.5f,  0.5f, -0.5f,   0.f,  0.f, -1.f,
    -0.5f,  0.5f, -0.5f,   0.f,  0.f, -1.f,
    // left
    -0.5f, -0.5f, -0.5f,  -1.f,  0.f,  0.f,
    -0.5f, -0.5f,  0.5f,  -1.f,  0.f,  0.f,
    -0.5f,  0.5f,  0.5f,  -1.f,  0.f,  0.f,
    -0.5f,  0.5f, -0.5f,  -1.f,  0.f,  0.f,
    // right
     0.5f, -0.5f, -0.5f,   1.f,  0.f,  0.f,
     0.5f, -0.5f,  0.5f,   1.f,  0.f,  0.f,
     0.5f,  0.5f,  0.5f,   1.f,  0.f,  0.f,
     0.5f,  0.5f, -0.5f,   1.f,  0.f,  0.f,
    // top
    -0.5f,  0.5f, -0.5f,   0.f,  1.f,  0.f,
    -0.5f,  0.5f,  0.5f,   0.f,  1.f,  0.f,
     0.5f,  0.5f,  0.5f,   0.f,  1.f,  0.f,
     0.5f,  0.5f, -0.5f,   0.f,  1.f,  0.f,
    // bottom
    -0.5f, -0.5f, -0.5f,   0.f, -1.f,  0.f,
    -0.5f, -0.5f,  0.5f,   0.f, -1.f,  0.f,
     0.5f, -0.5f,  0.5f,   0.f, -1.f,  0.f,
     0.5f, -0.5f, -0.5f,   0.f, -1.f,  0.f,
};

static constexpr io::u32 cube_indices[] = {
     0,  1,  2,   2,  3,  0,
     4,  5,  6,   6,  7,  4,
     8,  9, 10,  10, 11,  8,
    12, 13, 14,  14, 15, 12,
    16, 17, 18,  18, 19, 16,
    20, 21, 22,  22, 23, 20,
};

static constexpr SceneCube menu_cubes[] = {
    { -2.1f, -0.8f,  0.0f, 0.32f, 0.73f, 0.11f },
    { -0.9f,  0.7f, -1.3f, 0.67f, 0.39f, 0.21f },
    {  0.4f, -1.1f, -0.7f, 0.44f, 0.58f, 0.31f },
    {  1.7f,  0.4f, -1.9f, 0.56f, 0.47f, 0.16f },
    {  2.9f, -0.2f, -0.6f, 0.28f, 0.81f, 0.09f },
};

static inline lm::mat4 ModelMatrix(float x, float y, float z, float rx, float ry, float rz) noexcept {
    const lm::mat4 t = lm::mat4_translate(x, y, z);
    const lm::mat4 r = lm::mat4_rotate_z(rz) * (lm::mat4_rotate_y(ry) * lm::mat4_rotate_x(rx));
    return t * r;
}
