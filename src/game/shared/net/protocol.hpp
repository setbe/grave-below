#pragma once

#include "../../../3rd_party/hi/hi/socket.hpp"
#include "../../../engine/voxel/chunk.hpp"
#include "../world/item.hpp"
#include "../world/region.hpp"

namespace ge {
namespace net {
    static constexpr io::u16 GAME_UDP_PORT = 25565u;

    static constexpr io::u8 PK_C2S_PING = 32u;
    static constexpr io::u8 PK_S2C_PONG = 33u;
    static constexpr io::u8 PK_C2S_REQUEST_CHUNK = 34u;
    static constexpr io::u8 PK_S2C_CHUNK_BEGIN = 35u;
    static constexpr io::u8 PK_S2C_CHUNK_PART = 36u;
    static constexpr io::u8 PK_S2C_CHUNK_END = 37u;
    static constexpr io::u8 PK_C2S_PLAYER_POSITION = 38u;
    static constexpr io::u8 PK_S2C_PLAYER_POSITION = 39u;
    static constexpr io::u8 PK_C2S_CHUNK_ACK = 40u;
    static constexpr io::u8 PK_C2S_BLOCK_EDIT = 41u;
    static constexpr io::u8 PK_S2C_BLOCK_EDIT = 42u;
    static constexpr io::u8 PK_C2S_CHAT = 43u;
    static constexpr io::u8 PK_S2C_CHAT = 44u;
    static constexpr io::u8 PK_S2C_SERVER_TPS = 45u;
    static constexpr io::u8 PK_S2C_PLAYER_HEALTH = 46u;
    static constexpr io::u8 PK_S2C_WORLD_ACTOR = 47u;
    static constexpr io::u8 PK_S2C_WORLD_TIME = 48u;
    static constexpr io::u8 PK_C2S_INVENTORY_ACTION = 49u;
    static constexpr io::u8 PK_S2C_INVENTORY_STATE = 50u;
    static constexpr io::u8 PK_C2S_PLAYER_ROSTER_REQUEST = 51u;
    static constexpr io::u8 PK_C2S_PLAYER_ROSTER_SELF = 52u;
    static constexpr io::u8 PK_S2C_PLAYER_ROSTER_PAGE = 53u;
    static constexpr io::u8 PK_S2C_PLAYER_ROSTER_ADD = 54u;
    static constexpr io::u8 PK_S2C_REMOTE_PLAYER_POSE = 55u;
    static constexpr io::u8 PK_S2C_PLAYER_ROSTER_REMOVE = 56u;
    static constexpr io::u8 PK_C2S_MELEE_ATTACK = 57u;
    static constexpr io::u8 PK_C2S_WARD_CONFIG_ACTION = 58u;
    static constexpr io::u8 PK_S2C_WARD_CONFIG_STATE = 59u;
    static constexpr io::u8 PK_S2C_REGION_STATE = 60u;

    static constexpr io::u32 CHAT_NAME_MAX = 32u;
    static constexpr io::u32 CHAT_TEXT_MAX = 192u;
    static constexpr io::u32 PLAYER_NICK_BYTES = 32u;
    static constexpr io::u32 PLAYER_ROSTER_CLIENT_CAP = 256u;
    static constexpr io::u32 PLAYER_ROSTER_PAGE_MAX_ENTRIES = 32u;
    static constexpr io::u32 PLAYER_PING_NIBBLE_BITS = 4u;
    static constexpr io::u32 REGION_SYNC_MAX_ENTRIES = region::CLIENT_SYNC_MAX_REGIONS;

    static constexpr io::u32 CHUNK_WIRE_BLOCK_BYTES = voxel::CHUNK_VOLUME;
    static constexpr io::u32 CHUNK_WIRE_BLOCK_BYTES_NIBBLE = (CHUNK_WIRE_BLOCK_BYTES + 1u) / 2u;
    static constexpr io::u32 CHUNK_WIRE_BLOCK_BYTES_RAW_STATE = voxel::CHUNK_VOLUME * 3u; // id(1) + state(2)
    static constexpr io::u16 CHUNK_PART_BYTES = 1024u;
    static constexpr io::u16 CHUNK_MAX_PARTS =
        static_cast<io::u16>((CHUNK_WIRE_BLOCK_BYTES_RAW_STATE + CHUNK_PART_BYTES - 1u) / CHUNK_PART_BYTES);
    static constexpr io::u8 CHUNK_WIRE_ENCODING_RAW = 0u;
    static constexpr io::u8 CHUNK_WIRE_ENCODING_NIBBLE = 1u;
    static constexpr io::u8 CHUNK_WIRE_ENCODING_RAW_STATE = 2u;
    static constexpr io::u8 CHUNK_WIRE_ENCODING_RLE_STATE = 3u;

    enum class DeathReason : io::u16 {
        None = 0u,
        Fall = 1u,
        Starvation = 2u,
        Unknown = 65535u
    };

    IO_NODISCARD static inline io::char_view DeathReasonStr(DeathReason dr) noexcept {
        switch (dr) {
        case DeathReason::None: return "none";
        case DeathReason::Fall: return "fall damage";
        case DeathReason::Starvation: return "starvation";
        case DeathReason::Unknown: return "unknown";
        default: return "unknown";
        }
    }

    IO_NODISCARD static inline io::u16 part_count_for(io::u32 total_bytes, io::u16 bytes_per_part) noexcept {
        if (bytes_per_part == 0u) return 0u;
        return static_cast<io::u16>((total_bytes + bytes_per_part - 1u) / bytes_per_part);
    }

#pragma pack(push, 1)
    struct C2S_Ping {
        io::u32 client_ms_be{};
    };

    struct S2C_Pong {
        io::u32 client_ms_be{};
        io::u32 server_uptime_ms_be{};
    };

    struct C2S_RequestChunk {
        io::u32 request_id_be{};
        io::u32 cx_be{};
        io::u32 cy_be{};
        io::u32 cz_be{};
        io::u8 lod{};
        io::u8 _pad[3]{};
    };

    struct S2C_ChunkBegin {
        io::u32 request_id_be{};
        io::u32 cx_be{};
        io::u32 cy_be{};
        io::u32 cz_be{};
        io::u32 total_bytes_be{};
        io::u16 part_count_be{};
        io::u16 bytes_per_part_be{};
        io::u8 encoding{};
        io::u8 _pad{};
    };

    struct S2C_ChunkPartHeader {
        io::u32 request_id_be{};
        io::u16 part_index_be{};
        io::u16 part_size_be{};
    };

    struct S2C_ChunkEnd {
        io::u32 request_id_be{};
        io::u32 total_bytes_be{};
        io::u32 hash_be{};
    };

    struct C2S_PlayerPosition {
        io::u32 client_ms_be{};
        io::u32 pos_x_be{};
        io::u32 pos_y_be{};
        io::u32 pos_z_be{};
        io::u32 yaw_be{};
        io::u32 pitch_be{};
        io::u8 action_flags{};
        io::u8 _pad[3]{};
    };

    struct C2S_MeleeAttack {
        io::u32 client_ms_be{};
        io::u32 yaw_be{};
        io::u32 pitch_be{};
    };

    struct S2C_PlayerPosition {
        io::u32 server_ms_be{};
        io::u32 pos_x_be{};
        io::u32 pos_y_be{};
        io::u32 pos_z_be{};
        io::u8 flags{};
        io::u8 _pad[3]{};
    };

    struct C2S_ChunkAck {
        io::u32 request_id_be{};
        io::u32 cx_be{};
        io::u32 cy_be{};
        io::u32 cz_be{};
    };

    struct C2S_BlockEdit {
        io::u32 wx_be{};
        io::u32 wy_be{};
        io::u32 wz_be{};
        io::u16 block_id_be{};
        io::u16 state_be{};
    };

    struct S2C_BlockEdit {
        io::u32 wx_be{};
        io::u32 wy_be{};
        io::u32 wz_be{};
        io::u16 block_id_be{};
        io::u16 state_be{};
    };

    struct C2S_Chat {
        io::u8 name_len{};
        io::u8 text_len{};
        io::u8 _pad[2]{};
        char name[CHAT_NAME_MAX]{};
        char text[CHAT_TEXT_MAX]{};
    };

    struct S2C_Chat {
        io::u8 kind{};
        io::u8 name_len{};
        io::u8 text_len{};
        io::u8 _pad{};
        char name[CHAT_NAME_MAX]{};
        char text[CHAT_TEXT_MAX]{};
    };

    struct S2C_ServerTps {
        io::u16 tps_x100_be{};
        io::u16 _pad{};
    };

    struct S2C_PlayerHealth {
        io::u16 hp_be{};
        io::u16 damage_x10_be{};
        io::u16 fall_blocks_x10_be{};
        io::u8 hunger{};
        io::u8 flags{};
        io::u16 death_reason_be{};
    };

    struct S2C_WorldActor {
        io::u16 actor_id_be{};
        io::u8 model{};
        io::u8 mode{};
        io::u8 state{};
        io::u8 anim{};
        io::u8 flags{};
        io::u8 _pad{};
        io::u32 pos_x_be{};
        io::u32 pos_y_be{};
        io::u32 pos_z_be{};
    };

    struct S2C_WorldTime {
        io::u32 cycle_pos_ms_be{};
        io::u32 day_ms_be{};
        io::u32 night_ms_be{};
    };

    struct ItemSlotWire {
        io::u8 item_id{};
        io::u8 _pad{};
        io::u16 count_be{};
        io::u16 freshness_be{};
    };

    struct C2S_InventoryAction {
        io::u8 action{};
        io::u8 src_region{};
        io::u8 src_index{};
        io::u8 dst_region{};
        io::u8 dst_index{};
        io::u8 amount{};
        io::u16 _pad{};
    };

    struct S2C_InventoryState {
        io::u8 selected_hotbar{};
        io::u8 flags{};
        io::u16 _pad{};
        ItemSlotWire hotbar[item::HOTBAR_SLOT_COUNT]{};
        ItemSlotWire blocks[item::INVENTORY_SLOT_COUNT]{};
        ItemSlotWire consumables[item::INVENTORY_SLOT_COUNT]{};
        ItemSlotWire materials[item::INVENTORY_SLOT_COUNT]{};
        ItemSlotWire spelling_wards[item::INVENTORY_SLOT_COUNT]{};
        ItemSlotWire spells[item::INVENTORY_SLOT_COUNT]{};
        ItemSlotWire cursor{};
        ItemSlotWire trash{};
    };

    struct C2S_WardConfigAction {
        io::u16 ward_instance_be{};
        io::u8 ward_slot{};
        io::u8 op{};
    };

    struct S2C_WardConfigState {
        io::u16 ward_instance_be{};
        io::u8 slots_available{};
        io::u8 flags{};
        io::u16 stat_speed_x100_be{};
        io::u16 stat_delay_cast_x1000_be{};
        io::u16 stat_delay_reload_x1000_be{};
        io::u16 stat_spread_x100_be{};
        ItemSlotWire spells[item::INVENTORY_SLOT_COUNT]{};
    };

    struct C2S_PlayerRosterRequest {
        io::u16 start_index_be{};
        io::u16 max_entries_be{};
    };

    struct C2S_PlayerRosterSelf {
        io::u8 name_len{};
        io::u8 signal_quality{};
        io::u16 _pad{};
        char name[PLAYER_NICK_BYTES]{};
    };

    struct S2C_PlayerRosterPage {
        io::u16 start_index_be{};
        io::u16 total_online_be{};
        io::u8 count{};
        io::u8 flags{};
        io::u16 _pad{};
    };

    struct PlayerRosterEntryWire {
        io::u16 server_index_be{};
        io::u8 name_len{};
        io::u8 signal_quality{};
        char name[PLAYER_NICK_BYTES]{};
    };

    struct S2C_PlayerRosterAdd {
        io::u16 server_index_be{};
        io::u8 name_len{};
        io::u8 signal_quality{};
        char name[PLAYER_NICK_BYTES]{};
    };

    struct S2C_PlayerRosterRemove {
        io::u16 server_index_be{};
        io::u16 _pad{};
    };

    struct S2C_RemotePlayerPose {
        io::u16 server_index_be{};
        io::u8 state{};
        io::u8 action_flags{};
        io::u8 held_item_id{};
        io::u8 _pad[3]{};
        io::u32 pos_x_be{};
        io::u32 pos_y_be{};
        io::u32 pos_z_be{};
        io::u32 yaw_be{};
        io::u32 pitch_be{};
    };

    struct RegionEntryWire {
        io::u32 region_hi_be{};
        io::u32 region_lo_be{};
        io::u16 mana_be{};
        io::u16 instability_be{};
        io::u16 decay_be{};
        io::u8 bands{};
        io::u8 flags{};
    };

    struct S2C_RegionState {
        io::u8 count{};
        io::u8 _pad[3]{};
        RegionEntryWire entries[REGION_SYNC_MAX_ENTRIES]{};
    };
#pragma pack(pop)

    static_assert(sizeof(C2S_Ping) == 4, "C2S_Ping size");
    static_assert(sizeof(S2C_Pong) == 8, "S2C_Pong size");
    static_assert(sizeof(C2S_RequestChunk) == 20, "C2S_RequestChunk size");
    static_assert(sizeof(S2C_ChunkBegin) == 26, "S2C_ChunkBegin size");
    static_assert(sizeof(S2C_ChunkPartHeader) == 8, "S2C_ChunkPartHeader size");
    static_assert(sizeof(S2C_ChunkEnd) == 12, "S2C_ChunkEnd size");
    static_assert(sizeof(C2S_PlayerPosition) == 28, "C2S_PlayerPosition size");
    static_assert(sizeof(C2S_MeleeAttack) == 12, "C2S_MeleeAttack size");
    static_assert(sizeof(S2C_PlayerPosition) == 20, "S2C_PlayerPosition size");
    static_assert(sizeof(C2S_ChunkAck) == 16, "C2S_ChunkAck size");
    static_assert(sizeof(C2S_BlockEdit) == 16, "C2S_BlockEdit size");
    static_assert(sizeof(S2C_BlockEdit) == 16, "S2C_BlockEdit size");
    static_assert(sizeof(C2S_Chat) == 228, "C2S_Chat size");
    static_assert(sizeof(S2C_Chat) == 228, "S2C_Chat size");
    static_assert(sizeof(S2C_ServerTps) == 4, "S2C_ServerTps size");
    static_assert(sizeof(S2C_PlayerHealth) == 10, "S2C_PlayerHealth size");
    static_assert(sizeof(S2C_WorldActor) == 20, "S2C_WorldActor size");
    static_assert(sizeof(S2C_WorldTime) == 12, "S2C_WorldTime size");
    static_assert(sizeof(ItemSlotWire) == 6, "ItemSlotWire size");
    static_assert(sizeof(C2S_InventoryAction) == 8, "C2S_InventoryAction size");
    static_assert(sizeof(S2C_InventoryState) == 880, "S2C_InventoryState size");
    static_assert(sizeof(C2S_WardConfigAction) == 4, "C2S_WardConfigAction size");
    static_assert(sizeof(S2C_WardConfigState) == 174, "S2C_WardConfigState size");
    static_assert(sizeof(C2S_PlayerRosterRequest) == 4, "C2S_PlayerRosterRequest size");
    static_assert(sizeof(C2S_PlayerRosterSelf) == 36, "C2S_PlayerRosterSelf size");
    static_assert(sizeof(S2C_PlayerRosterPage) == 8, "S2C_PlayerRosterPage size");
    static_assert(sizeof(PlayerRosterEntryWire) == 36, "PlayerRosterEntryWire size");
    static_assert(sizeof(S2C_PlayerRosterAdd) == 36, "S2C_PlayerRosterAdd size");
    static_assert(sizeof(S2C_PlayerRosterRemove) == 4, "S2C_PlayerRosterRemove size");
    static_assert(sizeof(S2C_RemotePlayerPose) == 28, "S2C_RemotePlayerPose size");
    static_assert(sizeof(RegionEntryWire) == 16, "RegionEntryWire size");
    static_assert(sizeof(S2C_RegionState) == 4 + sizeof(RegionEntryWire) * REGION_SYNC_MAX_ENTRIES, "S2C_RegionState size");

    enum : io::u8 {
        PLAYER_POS_FLAG_CORRECTION = 1u << 0
    };

    enum : io::u8 {
        PLAYER_ACTION_FLAG_EAT = 1u << 0,
        PLAYER_ACTION_FLAG_SNEAK = 1u << 1,
        PLAYER_ACTION_FLAG_CRAWL = 1u << 2
    };

    enum : io::u8 {
        PLAYER_ACTION_FLAG_MASK = PLAYER_ACTION_FLAG_EAT | PLAYER_ACTION_FLAG_SNEAK | PLAYER_ACTION_FLAG_CRAWL
    };

    enum : io::u8 {
        PLAYER_ANIM_STILL = 0u,
        PLAYER_ANIM_WALK = 1u,
        PLAYER_ANIM_RUN = 2u,
        PLAYER_ANIM_EAT = 3u,
        PLAYER_ANIM_CRAWL_IDLE = 4u,
        PLAYER_ANIM_CRAWL_MOVE = 5u,
        PLAYER_ANIM_CRAWL_DOWN = 6u,
        PLAYER_ANIM_CRAWL_UP = 7u
    };

    enum : io::u8 {
        PLAYER_HEALTH_FLAG_DEAD = 1u << 0,
        PLAYER_HEALTH_FLAG_RESPAWN_RESET = 1u << 1
    };

    enum : io::u8 {
        CHAT_KIND_PLAYER = 0u,
        CHAT_KIND_SERVER = 1u
    };

    enum : io::u8 {
        WORLD_ACTOR_MODEL_NONE = 0u,
        WORLD_ACTOR_MODEL_LEVITATING_BOOK = 1u,
        WORLD_ACTOR_MODEL_ITEM = 2u,
        WORLD_ACTOR_MODEL_SPELL = 3u
    };

    enum : io::u8 {
        WORLD_ACTOR_MODE_ENTITY = 0u,
        WORLD_ACTOR_MODE_MOB = 1u
    };

    enum : io::u8 {
        INVENTORY_ACTION_SELECT_HOTBAR = 0u,
        INVENTORY_ACTION_MOVE = 1u,
        INVENTORY_ACTION_USE_SELECTED = 2u,
        INVENTORY_ACTION_LEFT_CLICK = 3u,
        INVENTORY_ACTION_RIGHT_CLICK = 4u,
        INVENTORY_ACTION_DROP_SLOT_STACK = 5u,
        INVENTORY_ACTION_DROP_SLOT_ONE = 6u,
        INVENTORY_ACTION_DROP_CURSOR_STACK = 7u,
        INVENTORY_ACTION_DROP_CURSOR_ONE = 8u
    };

    enum : io::u8 {
        INVENTORY_FLAG_CONTAINER_OPEN = 1u << 0
    };

    enum : io::u8 {
        WARD_CONFIG_OP_LEFT_CLICK = 0u,
        WARD_CONFIG_OP_RIGHT_CLICK = 1u
    };

    enum : io::u8 {
        WARD_CONFIG_FLAG_VALID = 1u << 0
    };

    enum : io::u8 {
        PLAYER_ROSTER_PAGE_FLAG_RESET = 1u << 0
    };

    enum : io::u8 {
        REGION_ENTRY_FLAG_CURRENT = 1u << 0,
        REGION_ENTRY_FLAG_NEIGHBOR = 1u << 1,
        REGION_ENTRY_FLAG_VERTICAL = 1u << 2
    };

    enum : io::u8 {
        WORLD_ACTOR_ANIM_STAY = 0u,
        WORLD_ACTOR_ANIM_LEVITATE = 1u
    };

    enum : io::u8 {
        WORLD_ACTOR_STATE_ENTITY_STAY = 0u,
        WORLD_ACTOR_STATE_ENTITY_LEVITATE = 1u,
        WORLD_ACTOR_STATE_MOB_IDLE = 2u,
        WORLD_ACTOR_STATE_MOB_CHASE = 3u
    };

    enum : io::u8 {
        WORLD_ACTOR_FLAG_ACTIVE = 1u << 0,
        WORLD_ACTOR_FLAG_GROUNDED = 1u << 1
    };

    struct ChunkRequest {
        io::u32 request_id{};
        io::i32 cx{};
        io::i32 cy{};
        io::i32 cz{};
        io::u8 lod{};
    };

    struct ChunkBegin {
        io::u32 request_id{};
        voxel::ChunkCoord coord{};
        io::u32 total_bytes{};
        io::u16 part_count{};
        io::u16 bytes_per_part{};
        io::u8 encoding = CHUNK_WIRE_ENCODING_RAW;
    };

    struct ChunkPart {
        io::u32 request_id{};
        io::u16 part_index{};
        io::u16 part_size{};
    };

    struct ChunkEnd {
        io::u32 request_id{};
        io::u32 total_bytes{};
        io::u32 hash{};
    };

    struct PlayerPositionSample {
        io::u32 client_ms{};
        float x{};
        float y{};
        float z{};
        float yaw{};
        float pitch{};
        io::u8 action_flags = 0u;
    };

    struct ServerPlayerPosition {
        io::u32 server_ms{};
        float x{};
        float y{};
        float z{};
        io::u8 flags{};
    };

    struct RemotePlayerPoseSample {
        io::u16 server_index = 0u;
        io::u8 state = PLAYER_ANIM_STILL;
        io::u8 action_flags = 0u;
        item::Id held_item = item::Id::None;
        float x{};
        float y{};
        float z{};
        float yaw{};
        float pitch{};
    };

    struct MeleeAttackSample {
        io::u32 client_ms{};
        float yaw{};
        float pitch{};
    };

    struct ChunkAck {
        io::u32 request_id{};
        voxel::ChunkCoord coord{};
    };

    struct BlockEdit {
        io::i32 wx{};
        io::i32 wy{};
        io::i32 wz{};
        io::u16 block_id{};
        io::u16 state{};
    };

    struct ChatLine {
        io::u8 kind = CHAT_KIND_PLAYER;
        io::u8 name_len{};
        io::u8 text_len{};
        char name[CHAT_NAME_MAX]{};
        char text[CHAT_TEXT_MAX]{};
    };

    struct ServerTpsSample {
        io::u16 tps_x100{};
    };

    struct PlayerHealthSample {
        io::u16 hp{};
        float damage{};
        float fall_blocks{};
        io::u8 hunger = 255u;
        io::u8 flags = 0u;
        DeathReason death_reason = DeathReason::None;
    };

    struct WorldActorSample {
        io::u16 actor_id{};
        io::u8 model = WORLD_ACTOR_MODEL_NONE;
        io::u8 mode = WORLD_ACTOR_MODE_ENTITY;
        io::u8 state = WORLD_ACTOR_STATE_ENTITY_STAY;
        io::u8 anim = WORLD_ACTOR_ANIM_STAY;
        io::u8 flags = 0u;
        float x{};
        float y{};
        float z{};
    };

    struct WorldTimeSample {
        io::u32 cycle_pos_ms{};
        io::u32 day_ms{};
        io::u32 night_ms{};
    };

    struct InventoryAction {
        io::u8 action = INVENTORY_ACTION_SELECT_HOTBAR;
        item::SlotRegion src_region = item::SlotRegion::Hotbar;
        io::u8 src_index = 0u;
        item::SlotRegion dst_region = item::SlotRegion::Hotbar;
        io::u8 dst_index = 0u;
        io::u8 amount = 0u;
    };

    struct InventoryStateSample {
        item::PlayerInventory inventory{};
        io::u8 flags = 0u;
    };

    struct WardConfigActionSample {
        io::u16 ward_instance = 0u;
        io::u8 ward_slot = 0u;
        bool right_click = false;
    };

    struct WardConfigStateSample {
        io::u16 ward_instance = 0u;
        io::u8 slots_available = 0u;
        bool valid = false;
        float stat_speed = 0.f;
        float stat_delay_cast = 0.f;
        float stat_delay_reload = 0.f;
        float stat_spread = 0.f;
        item::Stack spells[item::INVENTORY_SLOT_COUNT]{};
    };

    enum class SignalQuality : io::u8 {
        Bad = 0u,
        Okay = 1u,
        Good = 2u,
        Excellent = 3u
    };

    struct PlayerRosterRequest {
        io::u16 start_index = 0u;
        io::u16 max_entries = 0u;
    };

    struct PlayerRosterSelf {
        io::u8 name_len = 0u;
        char name[PLAYER_NICK_BYTES]{};
        SignalQuality signal_quality = SignalQuality::Bad;
    };

    struct PlayerRosterEntry {
        io::u16 server_index = 0u;
        io::u8 name_len = 0u;
        char name[PLAYER_NICK_BYTES]{};
        SignalQuality signal_quality = SignalQuality::Bad;
    };

    struct PlayerRosterPage {
        io::u16 start_index = 0u;
        io::u16 total_online = 0u;
        io::u8 count = 0u;
        io::u8 flags = 0u;
        PlayerRosterEntry entries[PLAYER_ROSTER_PAGE_MAX_ENTRIES]{};
    };

    struct PlayerRosterRemove {
        io::u16 server_index = 0u;
    };

    struct RegionEntrySample {
        region::RegionId region_id = 0ull;
        io::u16 mana = 0u;
        io::u16 instability = 0u;
        io::u16 decay = 0u;
        io::u8 bands = 0u;
        io::u8 flags = 0u;
    };

    struct RegionStateSample {
        io::u8 count = 0u;
        RegionEntrySample entries[REGION_SYNC_MAX_ENTRIES]{};
    };

    IO_NODISCARD static inline io::u8 signal_quality_nibble(SignalQuality q) noexcept {
        return static_cast<io::u8>(q) & 0x0Fu;
    }

    IO_NODISCARD static inline SignalQuality signal_quality_from_nibble(io::u8 nibble) noexcept {
        switch (nibble & 0x03u) {
        case 0u: return SignalQuality::Bad;
        case 1u: return SignalQuality::Okay;
        case 2u: return SignalQuality::Good;
        default: return SignalQuality::Excellent;
        }
    }

    IO_NODISCARD static inline SignalQuality signal_quality_from_ping_ms(io::u32 ping_ms) noexcept {
        if (ping_ms <= 60u) return SignalQuality::Excellent;
        if (ping_ms <= 110u) return SignalQuality::Good;
        if (ping_ms <= 180u) return SignalQuality::Okay;
        return SignalQuality::Bad;
    }

    IO_NODISCARD static inline io::u8 clamp_player_name_len(io::u8 len) noexcept {
        return len > PLAYER_NICK_BYTES ? static_cast<io::u8>(PLAYER_NICK_BYTES) : len;
    }

    IO_NODISCARD static inline io::u32 f32_to_bits(float value) noexcept {
        union {
            float f;
            io::u32 u;
        } conv{};
        conv.f = value;
        return conv.u;
    }

    IO_NODISCARD static inline float bits_to_f32(io::u32 value) noexcept {
        union {
            io::u32 u;
            float f;
        } conv{};
        conv.u = value;
        return conv.f;
    }

    IO_NODISCARD static inline io::u8 clamp_chat_len(io::u32 value, io::u32 max_value) noexcept {
        if (value > max_value) value = max_value;
        return static_cast<io::u8>(value);
    }

    static inline void encode_request(const ChunkRequest& in, C2S_RequestChunk& out) noexcept {
        out.request_id_be = io::h2nl(in.request_id);
        out.cx_be = io::h2nl(static_cast<io::u32>(in.cx));
        out.cy_be = io::h2nl(static_cast<io::u32>(in.cy));
        out.cz_be = io::h2nl(static_cast<io::u32>(in.cz));
        out.lod = in.lod;
        out._pad[0] = 0;
        out._pad[1] = 0;
        out._pad[2] = 0;
    }

    IO_NODISCARD static inline ChunkRequest decode_request(const C2S_RequestChunk& in) noexcept {
        ChunkRequest out{};
        out.request_id = io::n2hl(in.request_id_be);
        out.cx = static_cast<io::i32>(io::n2hl(in.cx_be));
        out.cy = static_cast<io::i32>(io::n2hl(in.cy_be));
        out.cz = static_cast<io::i32>(io::n2hl(in.cz_be));
        out.lod = in.lod;
        return out;
    }

    static inline void encode_chunk_begin(const ChunkBegin& in, S2C_ChunkBegin& out) noexcept {
        out.request_id_be = io::h2nl(in.request_id);
        out.cx_be = io::h2nl(static_cast<io::u32>(in.coord.x));
        out.cy_be = io::h2nl(static_cast<io::u32>(in.coord.y));
        out.cz_be = io::h2nl(static_cast<io::u32>(in.coord.z));
        out.total_bytes_be = io::h2nl(in.total_bytes);
        out.part_count_be = io::h2ns(in.part_count);
        out.bytes_per_part_be = io::h2ns(in.bytes_per_part);
        out.encoding = in.encoding;
        out._pad = 0u;
    }

    IO_NODISCARD static inline ChunkBegin decode_chunk_begin(const S2C_ChunkBegin& in) noexcept {
        ChunkBegin out{};
        out.request_id = io::n2hl(in.request_id_be);
        out.coord.x = static_cast<io::i32>(io::n2hl(in.cx_be));
        out.coord.y = static_cast<io::i32>(io::n2hl(in.cy_be));
        out.coord.z = static_cast<io::i32>(io::n2hl(in.cz_be));
        out.total_bytes = io::n2hl(in.total_bytes_be);
        out.part_count = io::n2hs(in.part_count_be);
        out.bytes_per_part = io::n2hs(in.bytes_per_part_be);
        out.encoding = in.encoding;
        return out;
    }

    static inline void encode_chunk_part(const ChunkPart& in, S2C_ChunkPartHeader& out) noexcept {
        out.request_id_be = io::h2nl(in.request_id);
        out.part_index_be = io::h2ns(in.part_index);
        out.part_size_be = io::h2ns(in.part_size);
    }

    IO_NODISCARD static inline ChunkPart decode_chunk_part(const S2C_ChunkPartHeader& in) noexcept {
        ChunkPart out{};
        out.request_id = io::n2hl(in.request_id_be);
        out.part_index = io::n2hs(in.part_index_be);
        out.part_size = io::n2hs(in.part_size_be);
        return out;
    }

    static inline void encode_chunk_end(const ChunkEnd& in, S2C_ChunkEnd& out) noexcept {
        out.request_id_be = io::h2nl(in.request_id);
        out.total_bytes_be = io::h2nl(in.total_bytes);
        out.hash_be = io::h2nl(in.hash);
    }

    IO_NODISCARD static inline ChunkEnd decode_chunk_end(const S2C_ChunkEnd& in) noexcept {
        ChunkEnd out{};
        out.request_id = io::n2hl(in.request_id_be);
        out.total_bytes = io::n2hl(in.total_bytes_be);
        out.hash = io::n2hl(in.hash_be);
        return out;
    }

    static inline void encode_player_position(const PlayerPositionSample& in, C2S_PlayerPosition& out) noexcept {
        out.client_ms_be = io::h2nl(in.client_ms);
        out.pos_x_be = io::h2nl(f32_to_bits(in.x));
        out.pos_y_be = io::h2nl(f32_to_bits(in.y));
        out.pos_z_be = io::h2nl(f32_to_bits(in.z));
        out.yaw_be = io::h2nl(f32_to_bits(in.yaw));
        out.pitch_be = io::h2nl(f32_to_bits(in.pitch));
        out.action_flags = static_cast<io::u8>(in.action_flags & PLAYER_ACTION_FLAG_MASK);
        out._pad[0] = 0u;
        out._pad[1] = 0u;
        out._pad[2] = 0u;
    }

    static inline void encode_c2s_melee_attack(const MeleeAttackSample& in, C2S_MeleeAttack& out) noexcept {
        out.client_ms_be = io::h2nl(in.client_ms);
        out.yaw_be = io::h2nl(f32_to_bits(in.yaw));
        out.pitch_be = io::h2nl(f32_to_bits(in.pitch));
    }

    IO_NODISCARD static inline MeleeAttackSample decode_c2s_melee_attack(const C2S_MeleeAttack& in) noexcept {
        MeleeAttackSample out{};
        out.client_ms = io::n2hl(in.client_ms_be);
        out.yaw = bits_to_f32(io::n2hl(in.yaw_be));
        out.pitch = bits_to_f32(io::n2hl(in.pitch_be));
        return out;
    }

    IO_NODISCARD static inline PlayerPositionSample decode_player_position(const C2S_PlayerPosition& in) noexcept {
        PlayerPositionSample out{};
        out.client_ms = io::n2hl(in.client_ms_be);
        out.x = bits_to_f32(io::n2hl(in.pos_x_be));
        out.y = bits_to_f32(io::n2hl(in.pos_y_be));
        out.z = bits_to_f32(io::n2hl(in.pos_z_be));
        out.yaw = bits_to_f32(io::n2hl(in.yaw_be));
        out.pitch = bits_to_f32(io::n2hl(in.pitch_be));
        out.action_flags = static_cast<io::u8>(in.action_flags & PLAYER_ACTION_FLAG_MASK);
        return out;
    }

    static inline void encode_server_player_position(const ServerPlayerPosition& in, S2C_PlayerPosition& out) noexcept {
        out.server_ms_be = io::h2nl(in.server_ms);
        out.pos_x_be = io::h2nl(f32_to_bits(in.x));
        out.pos_y_be = io::h2nl(f32_to_bits(in.y));
        out.pos_z_be = io::h2nl(f32_to_bits(in.z));
        out.flags = in.flags;
        out._pad[0] = 0u;
        out._pad[1] = 0u;
        out._pad[2] = 0u;
    }

    IO_NODISCARD static inline ServerPlayerPosition decode_server_player_position(const S2C_PlayerPosition& in) noexcept {
        ServerPlayerPosition out{};
        out.server_ms = io::n2hl(in.server_ms_be);
        out.x = bits_to_f32(io::n2hl(in.pos_x_be));
        out.y = bits_to_f32(io::n2hl(in.pos_y_be));
        out.z = bits_to_f32(io::n2hl(in.pos_z_be));
        out.flags = in.flags;
        return out;
    }

    static inline void encode_s2c_remote_player_pose(const RemotePlayerPoseSample& in, S2C_RemotePlayerPose& out) noexcept {
        out.server_index_be = io::h2ns(in.server_index);
        out.state = in.state;
        out.action_flags = static_cast<io::u8>(in.action_flags & PLAYER_ACTION_FLAG_MASK);
        out.held_item_id = static_cast<io::u8>(in.held_item);
        out._pad[0] = 0u;
        out._pad[1] = 0u;
        out._pad[2] = 0u;
        out.pos_x_be = io::h2nl(f32_to_bits(in.x));
        out.pos_y_be = io::h2nl(f32_to_bits(in.y));
        out.pos_z_be = io::h2nl(f32_to_bits(in.z));
        out.yaw_be = io::h2nl(f32_to_bits(in.yaw));
        out.pitch_be = io::h2nl(f32_to_bits(in.pitch));
    }

    IO_NODISCARD static inline RemotePlayerPoseSample decode_s2c_remote_player_pose(const S2C_RemotePlayerPose& in) noexcept {
        RemotePlayerPoseSample out{};
        out.server_index = io::n2hs(in.server_index_be);
        out.state = in.state;
        out.action_flags = static_cast<io::u8>(in.action_flags & PLAYER_ACTION_FLAG_MASK);
        out.held_item = static_cast<item::Id>(in.held_item_id);
        if (!item::valid(out.held_item)) out.held_item = item::Id::None;
        out.x = bits_to_f32(io::n2hl(in.pos_x_be));
        out.y = bits_to_f32(io::n2hl(in.pos_y_be));
        out.z = bits_to_f32(io::n2hl(in.pos_z_be));
        out.yaw = bits_to_f32(io::n2hl(in.yaw_be));
        out.pitch = bits_to_f32(io::n2hl(in.pitch_be));
        return out;
    }

    static inline void encode_chunk_ack(const ChunkAck& in, C2S_ChunkAck& out) noexcept {
        out.request_id_be = io::h2nl(in.request_id);
        out.cx_be = io::h2nl(static_cast<io::u32>(in.coord.x));
        out.cy_be = io::h2nl(static_cast<io::u32>(in.coord.y));
        out.cz_be = io::h2nl(static_cast<io::u32>(in.coord.z));
    }

    IO_NODISCARD static inline ChunkAck decode_chunk_ack(const C2S_ChunkAck& in) noexcept {
        ChunkAck out{};
        out.request_id = io::n2hl(in.request_id_be);
        out.coord.x = static_cast<io::i32>(io::n2hl(in.cx_be));
        out.coord.y = static_cast<io::i32>(io::n2hl(in.cy_be));
        out.coord.z = static_cast<io::i32>(io::n2hl(in.cz_be));
        return out;
    }

    static inline void encode_c2s_block_edit(const BlockEdit& in, C2S_BlockEdit& out) noexcept {
        out.wx_be = io::h2nl(static_cast<io::u32>(in.wx));
        out.wy_be = io::h2nl(static_cast<io::u32>(in.wy));
        out.wz_be = io::h2nl(static_cast<io::u32>(in.wz));
        out.block_id_be = io::h2ns(in.block_id);
        out.state_be = io::h2ns(in.state);
    }

    IO_NODISCARD static inline BlockEdit decode_c2s_block_edit(const C2S_BlockEdit& in) noexcept {
        BlockEdit out{};
        out.wx = static_cast<io::i32>(io::n2hl(in.wx_be));
        out.wy = static_cast<io::i32>(io::n2hl(in.wy_be));
        out.wz = static_cast<io::i32>(io::n2hl(in.wz_be));
        out.block_id = io::n2hs(in.block_id_be);
        out.state = io::n2hs(in.state_be);
        return out;
    }

    static inline void encode_s2c_block_edit(const BlockEdit& in, S2C_BlockEdit& out) noexcept {
        out.wx_be = io::h2nl(static_cast<io::u32>(in.wx));
        out.wy_be = io::h2nl(static_cast<io::u32>(in.wy));
        out.wz_be = io::h2nl(static_cast<io::u32>(in.wz));
        out.block_id_be = io::h2ns(in.block_id);
        out.state_be = io::h2ns(in.state);
    }

    IO_NODISCARD static inline BlockEdit decode_s2c_block_edit(const S2C_BlockEdit& in) noexcept {
        BlockEdit out{};
        out.wx = static_cast<io::i32>(io::n2hl(in.wx_be));
        out.wy = static_cast<io::i32>(io::n2hl(in.wy_be));
        out.wz = static_cast<io::i32>(io::n2hl(in.wz_be));
        out.block_id = io::n2hs(in.block_id_be);
        out.state = io::n2hs(in.state_be);
        return out;
    }

    static inline void encode_c2s_chat(const ChatLine& in, C2S_Chat& out) noexcept {
        out.name_len = clamp_chat_len(in.name_len, CHAT_NAME_MAX);
        out.text_len = clamp_chat_len(in.text_len, CHAT_TEXT_MAX);
        out._pad[0] = out._pad[1] = 0u;

        for (io::u32 i = 0; i < CHAT_NAME_MAX; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
        for (io::u32 i = 0; i < CHAT_TEXT_MAX; ++i)
            out.text[i] = (i < out.text_len) ? in.text[i] : '\0';
    }

    IO_NODISCARD static inline ChatLine decode_c2s_chat(const C2S_Chat& in) noexcept {
        ChatLine out{};
        out.kind = CHAT_KIND_PLAYER;
        out.name_len = clamp_chat_len(in.name_len, CHAT_NAME_MAX);
        out.text_len = clamp_chat_len(in.text_len, CHAT_TEXT_MAX);
        for (io::u32 i = 0; i < CHAT_NAME_MAX; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
        for (io::u32 i = 0; i < CHAT_TEXT_MAX; ++i)
            out.text[i] = (i < out.text_len) ? in.text[i] : '\0';
        return out;
    }

    static inline void encode_s2c_chat(const ChatLine& in, S2C_Chat& out) noexcept {
        out.kind = in.kind;
        out.name_len = clamp_chat_len(in.name_len, CHAT_NAME_MAX);
        out.text_len = clamp_chat_len(in.text_len, CHAT_TEXT_MAX);
        out._pad = 0u;

        for (io::u32 i = 0; i < CHAT_NAME_MAX; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
        for (io::u32 i = 0; i < CHAT_TEXT_MAX; ++i)
            out.text[i] = (i < out.text_len) ? in.text[i] : '\0';
    }

    IO_NODISCARD static inline ChatLine decode_s2c_chat(const S2C_Chat& in) noexcept {
        ChatLine out{};
        out.kind = in.kind;
        out.name_len = clamp_chat_len(in.name_len, CHAT_NAME_MAX);
        out.text_len = clamp_chat_len(in.text_len, CHAT_TEXT_MAX);
        for (io::u32 i = 0; i < CHAT_NAME_MAX; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
        for (io::u32 i = 0; i < CHAT_TEXT_MAX; ++i)
            out.text[i] = (i < out.text_len) ? in.text[i] : '\0';
        return out;
    }

    static inline void encode_s2c_server_tps(const ServerTpsSample& in, S2C_ServerTps& out) noexcept {
        out.tps_x100_be = io::h2ns(in.tps_x100);
        out._pad = 0u;
    }

    IO_NODISCARD static inline ServerTpsSample decode_s2c_server_tps(const S2C_ServerTps& in) noexcept {
        ServerTpsSample out{};
        out.tps_x100 = io::n2hs(in.tps_x100_be);
        return out;
    }

    static inline void encode_s2c_player_health(const PlayerHealthSample& in, S2C_PlayerHealth& out) noexcept {
        io::u16 hp = in.hp;
        if (hp > 100u) hp = 100u;
        io::u32 damage_x10 = io::to_u32(in.damage * 10.f + 0.5f);
        io::u32 fall_x10 = io::to_u32(in.fall_blocks * 10.f + 0.5f);
        if (damage_x10 > 65535u) damage_x10 = 65535u;
        if (fall_x10 > 65535u) fall_x10 = 65535u;
        out.hp_be = io::h2ns(hp);
        out.damage_x10_be = io::h2ns(static_cast<io::u16>(damage_x10));
        out.fall_blocks_x10_be = io::h2ns(static_cast<io::u16>(fall_x10));
        out.hunger = in.hunger;
        out.flags = in.flags;
        out.death_reason_be = io::h2ns(static_cast<io::u16>(in.death_reason));
    }

    IO_NODISCARD static inline PlayerHealthSample decode_s2c_player_health(const S2C_PlayerHealth& in) noexcept {
        PlayerHealthSample out{};
        out.hp = io::n2hs(in.hp_be);
        out.damage = static_cast<float>(io::n2hs(in.damage_x10_be)) * 0.1f;
        out.fall_blocks = static_cast<float>(io::n2hs(in.fall_blocks_x10_be)) * 0.1f;
        out.hunger = in.hunger;
        out.flags = in.flags;
        out.death_reason = static_cast<DeathReason>(io::n2hs(in.death_reason_be));
        return out;
    }

    static inline void encode_s2c_world_actor(const WorldActorSample& in, S2C_WorldActor& out) noexcept {
        out.actor_id_be = io::h2ns(in.actor_id);
        out.model = in.model;
        out.mode = in.mode;
        out.state = in.state;
        out.anim = in.anim;
        out.flags = in.flags;
        out._pad = 0u;
        out.pos_x_be = io::h2nl(f32_to_bits(in.x));
        out.pos_y_be = io::h2nl(f32_to_bits(in.y));
        out.pos_z_be = io::h2nl(f32_to_bits(in.z));
    }

    IO_NODISCARD static inline WorldActorSample decode_s2c_world_actor(const S2C_WorldActor& in) noexcept {
        WorldActorSample out{};
        out.actor_id = io::n2hs(in.actor_id_be);
        out.model = in.model;
        out.mode = in.mode;
        out.state = in.state;
        out.anim = in.anim;
        out.flags = in.flags;
        out.x = bits_to_f32(io::n2hl(in.pos_x_be));
        out.y = bits_to_f32(io::n2hl(in.pos_y_be));
        out.z = bits_to_f32(io::n2hl(in.pos_z_be));
        return out;
    }

    static inline void encode_s2c_world_time(const WorldTimeSample& in, S2C_WorldTime& out) noexcept {
        out.cycle_pos_ms_be = io::h2nl(in.cycle_pos_ms);
        out.day_ms_be = io::h2nl(in.day_ms);
        out.night_ms_be = io::h2nl(in.night_ms);
    }

    IO_NODISCARD static inline WorldTimeSample decode_s2c_world_time(const S2C_WorldTime& in) noexcept {
        WorldTimeSample out{};
        out.cycle_pos_ms = io::n2hl(in.cycle_pos_ms_be);
        out.day_ms = io::n2hl(in.day_ms_be);
        out.night_ms = io::n2hl(in.night_ms_be);
        return out;
    }

    static inline void encode_item_slot(const item::Stack& in, ItemSlotWire& out) noexcept {
        out.item_id = static_cast<io::u8>(in.id);
        out._pad = 0u;
        out.count_be = io::h2ns(in.count);
        out.freshness_be = io::h2ns(in.freshness);
    }

    IO_NODISCARD static inline item::Stack decode_item_slot(const ItemSlotWire& in) noexcept {
        item::Stack out{};
        out.id = static_cast<item::Id>(in.item_id);
        out.count = io::n2hs(in.count_be);
        out.freshness = io::n2hs(in.freshness_be);
        item::normalize(out);
        return out;
    }

    IO_NODISCARD static inline io::u16 quantize_u16(float value, float scale) noexcept {
        if (!(value == value) || value <= 0.f) return 0u;
        const float scaled = value * scale + 0.5f;
        if (scaled >= 65535.f) return 65535u;
        return static_cast<io::u16>(scaled);
    }

    IO_NODISCARD static inline float dequantize_u16(io::u16 value, float inv_scale) noexcept {
        return static_cast<float>(value) * inv_scale;
    }

    static inline void encode_c2s_inventory_action(const InventoryAction& in, C2S_InventoryAction& out) noexcept {
        out.action = in.action;
        out.src_region = static_cast<io::u8>(in.src_region);
        out.src_index = in.src_index;
        out.dst_region = static_cast<io::u8>(in.dst_region);
        out.dst_index = in.dst_index;
        out.amount = in.amount;
        out._pad = 0u;
    }

    IO_NODISCARD static inline InventoryAction decode_c2s_inventory_action(const C2S_InventoryAction& in) noexcept {
        InventoryAction out{};
        out.action = in.action;
        out.src_region = static_cast<item::SlotRegion>(in.src_region);
        out.src_index = in.src_index;
        out.dst_region = static_cast<item::SlotRegion>(in.dst_region);
        out.dst_index = in.dst_index;
        out.amount = in.amount;
        return out;
    }

    static inline void encode_c2s_ward_config_action(const WardConfigActionSample& in, C2S_WardConfigAction& out) noexcept {
        out.ward_instance_be = io::h2ns(in.ward_instance);
        out.ward_slot = in.ward_slot;
        out.op = in.right_click ? WARD_CONFIG_OP_RIGHT_CLICK : WARD_CONFIG_OP_LEFT_CLICK;
    }

    IO_NODISCARD static inline WardConfigActionSample decode_c2s_ward_config_action(const C2S_WardConfigAction& in) noexcept {
        WardConfigActionSample out{};
        out.ward_instance = io::n2hs(in.ward_instance_be);
        out.ward_slot = in.ward_slot;
        out.right_click = (in.op == WARD_CONFIG_OP_RIGHT_CLICK);
        return out;
    }

    static inline void encode_s2c_inventory_state(const InventoryStateSample& in, S2C_InventoryState& out) noexcept {
        out.selected_hotbar = in.inventory.selected_hotbar;
        out.flags = in.flags;
        out._pad = 0u;
        for (io::u32 i = 0u; i < item::HOTBAR_SLOT_COUNT; ++i)
            encode_item_slot(in.inventory.hotbar[i], out.hotbar[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            encode_item_slot(in.inventory.blocks[i], out.blocks[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            encode_item_slot(in.inventory.consumables[i], out.consumables[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            encode_item_slot(in.inventory.materials[i], out.materials[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            encode_item_slot(in.inventory.spelling_wards[i], out.spelling_wards[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            encode_item_slot(in.inventory.spells[i], out.spells[i]);
        encode_item_slot(in.inventory.cursor, out.cursor);
        encode_item_slot(in.inventory.trash, out.trash);
    }

    IO_NODISCARD static inline InventoryStateSample decode_s2c_inventory_state(const S2C_InventoryState& in) noexcept {
        InventoryStateSample out{};
        out.inventory.selected_hotbar = in.selected_hotbar;
        out.flags = in.flags;
        for (io::u32 i = 0u; i < item::HOTBAR_SLOT_COUNT; ++i)
            out.inventory.hotbar[i] = decode_item_slot(in.hotbar[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            out.inventory.blocks[i] = decode_item_slot(in.blocks[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            out.inventory.consumables[i] = decode_item_slot(in.consumables[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            out.inventory.materials[i] = decode_item_slot(in.materials[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            out.inventory.spelling_wards[i] = decode_item_slot(in.spelling_wards[i]);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            out.inventory.spells[i] = decode_item_slot(in.spells[i]);
        out.inventory.cursor = decode_item_slot(in.cursor);
        out.inventory.trash = decode_item_slot(in.trash);
        return out;
    }

    static inline void encode_s2c_ward_config_state(const WardConfigStateSample& in, S2C_WardConfigState& out) noexcept {
        out.ward_instance_be = io::h2ns(in.ward_instance);
        out.slots_available = (in.slots_available > item::INVENTORY_SLOT_COUNT)
            ? static_cast<io::u8>(item::INVENTORY_SLOT_COUNT)
            : in.slots_available;
        out.flags = in.valid ? WARD_CONFIG_FLAG_VALID : 0u;
        out.stat_speed_x100_be = io::h2ns(quantize_u16(in.stat_speed, 100.f));
        out.stat_delay_cast_x1000_be = io::h2ns(quantize_u16(in.stat_delay_cast, 1000.f));
        out.stat_delay_reload_x1000_be = io::h2ns(quantize_u16(in.stat_delay_reload, 1000.f));
        out.stat_spread_x100_be = io::h2ns(quantize_u16(in.stat_spread, 100.f));
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            encode_item_slot(in.spells[i], out.spells[i]);
    }

    IO_NODISCARD static inline WardConfigStateSample decode_s2c_ward_config_state(const S2C_WardConfigState& in) noexcept {
        WardConfigStateSample out{};
        out.ward_instance = io::n2hs(in.ward_instance_be);
        out.slots_available = (in.slots_available > item::INVENTORY_SLOT_COUNT)
            ? static_cast<io::u8>(item::INVENTORY_SLOT_COUNT)
            : in.slots_available;
        out.valid = (in.flags & WARD_CONFIG_FLAG_VALID) != 0u;
        out.stat_speed = dequantize_u16(io::n2hs(in.stat_speed_x100_be), 0.01f);
        out.stat_delay_cast = dequantize_u16(io::n2hs(in.stat_delay_cast_x1000_be), 0.001f);
        out.stat_delay_reload = dequantize_u16(io::n2hs(in.stat_delay_reload_x1000_be), 0.001f);
        out.stat_spread = dequantize_u16(io::n2hs(in.stat_spread_x100_be), 0.01f);
        for (io::u32 i = 0u; i < item::INVENTORY_SLOT_COUNT; ++i)
            out.spells[i] = decode_item_slot(in.spells[i]);
        return out;
    }

    static inline void encode_c2s_player_roster_request(const PlayerRosterRequest& in, C2S_PlayerRosterRequest& out) noexcept {
        out.start_index_be = io::h2ns(in.start_index);
        out.max_entries_be = io::h2ns(in.max_entries);
    }

    IO_NODISCARD static inline PlayerRosterRequest decode_c2s_player_roster_request(const C2S_PlayerRosterRequest& in) noexcept {
        PlayerRosterRequest out{};
        out.start_index = io::n2hs(in.start_index_be);
        out.max_entries = io::n2hs(in.max_entries_be);
        return out;
    }

    static inline void encode_c2s_player_roster_self(const PlayerRosterSelf& in, C2S_PlayerRosterSelf& out) noexcept {
        out.name_len = clamp_player_name_len(in.name_len);
        out.signal_quality = signal_quality_nibble(in.signal_quality);
        out._pad = 0u;
        for (io::u32 i = 0u; i < PLAYER_NICK_BYTES; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
    }

    IO_NODISCARD static inline PlayerRosterSelf decode_c2s_player_roster_self(const C2S_PlayerRosterSelf& in) noexcept {
        PlayerRosterSelf out{};
        out.name_len = clamp_player_name_len(in.name_len);
        out.signal_quality = signal_quality_from_nibble(in.signal_quality);
        for (io::u32 i = 0u; i < PLAYER_NICK_BYTES; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
        return out;
    }

    static inline void encode_s2c_player_roster_page_header(const PlayerRosterPage& in, S2C_PlayerRosterPage& out) noexcept {
        out.start_index_be = io::h2ns(in.start_index);
        out.total_online_be = io::h2ns(in.total_online);
        out.count = (in.count > PLAYER_ROSTER_PAGE_MAX_ENTRIES) ? static_cast<io::u8>(PLAYER_ROSTER_PAGE_MAX_ENTRIES) : in.count;
        out.flags = in.flags;
        out._pad = 0u;
    }

    IO_NODISCARD static inline PlayerRosterPage decode_s2c_player_roster_page_header(const S2C_PlayerRosterPage& in) noexcept {
        PlayerRosterPage out{};
        out.start_index = io::n2hs(in.start_index_be);
        out.total_online = io::n2hs(in.total_online_be);
        out.count = (in.count > PLAYER_ROSTER_PAGE_MAX_ENTRIES) ? static_cast<io::u8>(PLAYER_ROSTER_PAGE_MAX_ENTRIES) : in.count;
        out.flags = in.flags;
        return out;
    }

    static inline void encode_player_roster_entry(const PlayerRosterEntry& in, PlayerRosterEntryWire& out) noexcept {
        out.server_index_be = io::h2ns(in.server_index);
        out.name_len = clamp_player_name_len(in.name_len);
        out.signal_quality = signal_quality_nibble(in.signal_quality);
        for (io::u32 i = 0u; i < PLAYER_NICK_BYTES; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
    }

    IO_NODISCARD static inline PlayerRosterEntry decode_player_roster_entry(const PlayerRosterEntryWire& in) noexcept {
        PlayerRosterEntry out{};
        out.server_index = io::n2hs(in.server_index_be);
        out.name_len = clamp_player_name_len(in.name_len);
        out.signal_quality = signal_quality_from_nibble(in.signal_quality);
        for (io::u32 i = 0u; i < PLAYER_NICK_BYTES; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
        return out;
    }

    static inline void encode_s2c_player_roster_add(const PlayerRosterEntry& in, S2C_PlayerRosterAdd& out) noexcept {
        out.server_index_be = io::h2ns(in.server_index);
        out.name_len = clamp_player_name_len(in.name_len);
        out.signal_quality = signal_quality_nibble(in.signal_quality);
        for (io::u32 i = 0u; i < PLAYER_NICK_BYTES; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
    }

    IO_NODISCARD static inline PlayerRosterEntry decode_s2c_player_roster_add(const S2C_PlayerRosterAdd& in) noexcept {
        PlayerRosterEntry out{};
        out.server_index = io::n2hs(in.server_index_be);
        out.name_len = clamp_player_name_len(in.name_len);
        out.signal_quality = signal_quality_from_nibble(in.signal_quality);
        for (io::u32 i = 0u; i < PLAYER_NICK_BYTES; ++i)
            out.name[i] = (i < out.name_len) ? in.name[i] : '\0';
        return out;
    }

    static inline void encode_s2c_player_roster_remove(const PlayerRosterRemove& in, S2C_PlayerRosterRemove& out) noexcept {
        out.server_index_be = io::h2ns(in.server_index);
        out._pad = 0u;
    }

    IO_NODISCARD static inline PlayerRosterRemove decode_s2c_player_roster_remove(const S2C_PlayerRosterRemove& in) noexcept {
        PlayerRosterRemove out{};
        out.server_index = io::n2hs(in.server_index_be);
        return out;
    }

    IO_NODISCARD static inline io::u32 region_id_hi(region::RegionId id) noexcept {
        return static_cast<io::u32>((id >> 32u) & 0xFFFFFFFFull);
    }

    IO_NODISCARD static inline io::u32 region_id_lo(region::RegionId id) noexcept {
        return static_cast<io::u32>(id & 0xFFFFFFFFull);
    }

    IO_NODISCARD static inline region::RegionId compose_region_id(io::u32 hi, io::u32 lo) noexcept {
        return (static_cast<region::RegionId>(hi) << 32u) | static_cast<region::RegionId>(lo);
    }

    static inline void encode_region_entry(const RegionEntrySample& in, RegionEntryWire& out) noexcept {
        out.region_hi_be = io::h2nl(region_id_hi(in.region_id));
        out.region_lo_be = io::h2nl(region_id_lo(in.region_id));
        out.mana_be = io::h2ns(in.mana);
        out.instability_be = io::h2ns(in.instability);
        out.decay_be = io::h2ns(in.decay);
        out.bands = in.bands;
        out.flags = in.flags;
    }

    IO_NODISCARD static inline RegionEntrySample decode_region_entry(const RegionEntryWire& in) noexcept {
        RegionEntrySample out{};
        out.region_id = compose_region_id(io::n2hl(in.region_hi_be), io::n2hl(in.region_lo_be));
        out.mana = io::n2hs(in.mana_be);
        out.instability = io::n2hs(in.instability_be);
        out.decay = io::n2hs(in.decay_be);
        out.bands = in.bands;
        out.flags = in.flags;
        return out;
    }

    static inline void encode_s2c_region_state(const RegionStateSample& in, S2C_RegionState& out) noexcept {
        out.count = (in.count > REGION_SYNC_MAX_ENTRIES) ? static_cast<io::u8>(REGION_SYNC_MAX_ENTRIES) : in.count;
        out._pad[0] = out._pad[1] = out._pad[2] = 0u;
        for (io::u32 i = 0u; i < REGION_SYNC_MAX_ENTRIES; ++i) {
            if (i < out.count)
                encode_region_entry(in.entries[i], out.entries[i]);
            else
                out.entries[i] = {};
        }
    }

    IO_NODISCARD static inline RegionStateSample decode_s2c_region_state(const S2C_RegionState& in) noexcept {
        RegionStateSample out{};
        out.count = (in.count > REGION_SYNC_MAX_ENTRIES) ? static_cast<io::u8>(REGION_SYNC_MAX_ENTRIES) : in.count;
        for (io::u32 i = 0u; i < out.count; ++i)
            out.entries[i] = decode_region_entry(in.entries[i]);
        return out;
    }
} // namespace net
} // namespace ge
