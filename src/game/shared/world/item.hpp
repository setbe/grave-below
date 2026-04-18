#pragma once

#include "hi/hi/hi.hpp"

#include "../../../engine/voxel/block.hpp"

namespace ge {
namespace item {
    enum class Category : io::u8 {
        Blocks = 0u,
        Consumables = 1u,
        Materials = 2u,
        SpellingWards = 3u,
        Spells = 4u
    };

    enum class FreshnessBand : io::u8 {
        Fresh = 0u,
        Stale = 1u,
        Rotten = 2u,
        __COUNT__
    };

    enum class Visual : io::u8 {
        None = 0u,
        GrassBlock = 1u,
        DirtBlock = 2u,
        StoneBlock = 3u,
        SandBlock = 4u,
        LogBlock = 5u,
        LeavesBlock = 6u,
        PotatoFresh = 7u,
        PotatoRotten = 8u,
        SpellWardBook = 9u,
        Water = 10u,
        Blood = 11u,
        Slime = 12u,
        Dagger = 13u,
        SpellBolt = 14u,
        SpellDig = 15u,
        SpellBurst = 16u,
        SpellBeam = 17u,
        SpellOrb = 18u,
        SpellMine = 19u,
        SpellShieldPulse = 20u,
        SpellMark = 21u,
        SpellPull = 22u,
        SpellBlinkStep = 23u
    };

    enum class Id : io::u8 {
        None = 0u,
        GrassBlock = 1u,
        DirtBlock = 2u,
        StoneBlock = 3u,
        SandBlock = 4u,
        LogBlock = 5u,
        LeavesBlock = 6u,
        Potato = 7u,
        SpellWard = 8u,
        WaterBucket = 9u,
        BloodBucket = 10u,
        SlimeBucket = 11u,
        RustyDagger = 12u,
        SpellBolt = 13u,
        SpellDig = 14u,
        SpellBurst = 15u,
        SpellBeam = 16u,
        SpellOrb = 17u,
        SpellMine = 18u,
        SpellShieldPulse = 19u,
        SpellMark = 20u,
        SpellPull = 21u,
        SpellBlinkStep = 22u,
        __COUNT__
    };

    static constexpr io::u8 ITEM_FLAG_PLACEABLE = 1u << 0;
    static constexpr io::u8 ITEM_FLAG_CONSUMABLE = 1u << 1;
    static constexpr io::u8 ITEM_FLAG_DECAYS = 1u << 2;
    static constexpr io::u8 ITEM_FLAG_ENTITY_BOOK = 1u << 3;
    static constexpr io::u8 ITEM_FLAG_INSTANCE_DATA = 1u << 4;

    static constexpr io::u16 FRESHNESS_MAX = 1000u;
    static constexpr io::u32 HOTBAR_SLOT_COUNT = 9u;
    static constexpr io::u32 INVENTORY_SLOT_COUNT = 27u;
    static constexpr io::u32 CONTAINER_SLOT_COUNT = 27u;
    static constexpr io::u32 PLAYER_STORAGE_SLOT_COUNT = INVENTORY_SLOT_COUNT * 3u;
    static constexpr io::u32 PLAYER_SLOT_COUNT = HOTBAR_SLOT_COUNT + PLAYER_STORAGE_SLOT_COUNT;

    enum class SlotRegion : io::u8 {
        Hotbar = 0u,
        Blocks = 1u,
        General = 2u,
        Consumables = 3u,
        Materials = 4u,
        SpellingWards = 5u,
        Spells = 6u,
        Container = 7u,
        Cursor = 8u,
        Trash = 9u
    };

    struct Stack {
        Id id = Id::None;
        io::u16 count = 0u;
        io::u16 freshness = 0u;
    };

    struct Def {
        const char* name = "None";
        const char* short_name = "--";
        Category category = Category::Blocks;
        Visual fresh_visual = Visual::None;
        Visual stale_visual = Visual::None;
        Visual rotten_visual = Visual::None;
        ge::voxel::BlockId place_block = ge::voxel::BlockId::Air;
        Id decay_result = Id::None;
        io::u16 max_stack = 1u;
        io::u16 hunger_gain = 0u;
        io::u16 poison_ms = 0u;
        io::u32 decay_ms = 0u;
        io::u8 flags = 0u;
    };

    static constexpr io::u32 ITEM_COUNT = static_cast<io::u32>(Id::__COUNT__);
    static constexpr io::u32 FRESHNESS_BAND_COUNT = static_cast<io::u32>(FreshnessBand::__COUNT__);

    static constexpr Def g_defs[ITEM_COUNT]{
        { "None", "--", Category::Blocks, Visual::None, Visual::None, Visual::None,
          ge::voxel::BlockId::Air, Id::None, 1u, 0u, 0u, 0u, 0u },
        { "Grass Block", "GR", Category::Blocks, Visual::GrassBlock, Visual::GrassBlock, Visual::GrassBlock,
          ge::voxel::BlockId::Grass, Id::None, 64u, 0u, 0u, 0u, ITEM_FLAG_PLACEABLE },
        { "Dirt Block", "DI", Category::Blocks, Visual::DirtBlock, Visual::DirtBlock, Visual::DirtBlock,
          ge::voxel::BlockId::Dirt, Id::None, 64u, 0u, 0u, 0u, ITEM_FLAG_PLACEABLE },
        { "Stone Block", "ST", Category::Blocks, Visual::StoneBlock, Visual::StoneBlock, Visual::StoneBlock,
          ge::voxel::BlockId::Stone, Id::None, 64u, 0u, 0u, 0u, ITEM_FLAG_PLACEABLE },
        { "Sand Block", "SA", Category::Blocks, Visual::SandBlock, Visual::SandBlock, Visual::SandBlock,
          ge::voxel::BlockId::Sand, Id::None, 64u, 0u, 0u, 0u, ITEM_FLAG_PLACEABLE },
        { "Log Block", "LG", Category::Blocks, Visual::LogBlock, Visual::LogBlock, Visual::LogBlock,
          ge::voxel::BlockId::Log, Id::None, 64u, 0u, 0u, 0u, ITEM_FLAG_PLACEABLE },
        { "Leaves Block", "LV", Category::Blocks, Visual::LeavesBlock, Visual::LeavesBlock, Visual::LeavesBlock,
          ge::voxel::BlockId::Leaves, Id::None, 64u, 0u, 0u, 0u, ITEM_FLAG_PLACEABLE },
        { "Potato", "PO", Category::Consumables, Visual::PotatoFresh, Visual::PotatoFresh, Visual::PotatoRotten,
          ge::voxel::BlockId::Air, Id::None, 16u, 24u, 10000u, 900000u, ITEM_FLAG_CONSUMABLE | ITEM_FLAG_DECAYS },
        { "Spell Ward", "SW", Category::SpellingWards, Visual::SpellWardBook, Visual::SpellWardBook, Visual::SpellWardBook,
          ge::voxel::BlockId::Air, Id::None, 1u, 0u, 0u, 0u, ITEM_FLAG_ENTITY_BOOK | ITEM_FLAG_INSTANCE_DATA },
        { "Water Bucket", "WA", Category::Consumables, Visual::Water, Visual::Water, Visual::Water,
          ge::voxel::BlockId::Air, Id::None, 1u, 0u, 0u, 0u, 0u },
        { "Blood Bucket", "BL", Category::Consumables, Visual::Blood, Visual::Blood, Visual::Blood,
          ge::voxel::BlockId::Air, Id::None, 1u, 0u, 0u, 0u, 0u },
        { "Slime Bucket", "SL", Category::Consumables, Visual::Slime, Visual::Slime, Visual::Slime,
          ge::voxel::BlockId::Air, Id::None, 1u, 0u, 0u, 0u, 0u },
        { "Rusty Dagger", "DG", Category::Materials, Visual::Dagger, Visual::Dagger, Visual::Dagger,
          ge::voxel::BlockId::Air, Id::None, 1u, 0u, 0u, 0u, 0u },
        { "Bolt Spell", "SB", Category::Spells, Visual::SpellBolt, Visual::SpellBolt, Visual::SpellBolt,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Dig Spell", "SD", Category::Spells, Visual::SpellDig, Visual::SpellDig, Visual::SpellDig,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Burst Spell", "SR", Category::Spells, Visual::SpellBurst, Visual::SpellBurst, Visual::SpellBurst,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Beam Spell", "SBM", Category::Spells, Visual::SpellBeam, Visual::SpellBeam, Visual::SpellBeam,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Orb Spell", "SOB", Category::Spells, Visual::SpellOrb, Visual::SpellOrb, Visual::SpellOrb,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Mine Spell", "SMN", Category::Spells, Visual::SpellMine, Visual::SpellMine, Visual::SpellMine,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Shield Pulse Spell", "SSP", Category::Spells, Visual::SpellShieldPulse, Visual::SpellShieldPulse, Visual::SpellShieldPulse,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Mark Spell", "SMK", Category::Spells, Visual::SpellMark, Visual::SpellMark, Visual::SpellMark,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Pull Spell", "SPL", Category::Spells, Visual::SpellPull, Visual::SpellPull, Visual::SpellPull,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u },
        { "Blink Step Spell", "SBS", Category::Spells, Visual::SpellBlinkStep, Visual::SpellBlinkStep, Visual::SpellBlinkStep,
          ge::voxel::BlockId::Air, Id::None, 16u, 0u, 0u, 0u, 0u }
    };

    struct PlayerInventory {
        Stack hotbar[HOTBAR_SLOT_COUNT]{};
        Stack blocks[INVENTORY_SLOT_COUNT]{};
        Stack consumables[INVENTORY_SLOT_COUNT]{};
        Stack materials[INVENTORY_SLOT_COUNT]{};
        Stack spelling_wards[INVENTORY_SLOT_COUNT]{};
        Stack spells[INVENTORY_SLOT_COUNT]{};
        Stack cursor{};
        Stack trash{};
        io::u8 selected_hotbar = 0u;
    };

    struct ContainerInventory {
        Stack slots[CONTAINER_SLOT_COUNT]{};
    };

    IO_NODISCARD static inline constexpr io::u32 index(Id id) noexcept {
        return static_cast<io::u32>(id);
    }

    IO_NODISCARD static inline constexpr bool valid(Id id) noexcept {
        return index(id) < ITEM_COUNT;
    }

    IO_NODISCARD static inline constexpr const Def& def(Id id) noexcept {
        return g_defs[valid(id) ? index(id) : 0u];
    }

    IO_NODISCARD static inline constexpr bool has_flag(Id id, io::u8 flag) noexcept {
        return (def(id).flags & flag) != 0u;
    }

    IO_NODISCARD static inline constexpr bool is_placeable(Id id) noexcept {
        return has_flag(id, ITEM_FLAG_PLACEABLE);
    }

    IO_NODISCARD static inline constexpr bool is_consumable(Id id) noexcept {
        return has_flag(id, ITEM_FLAG_CONSUMABLE);
    }

    IO_NODISCARD static inline constexpr bool decays(Id id) noexcept {
        return has_flag(id, ITEM_FLAG_DECAYS);
    }

    IO_NODISCARD static inline constexpr bool uses_book_model(Id id) noexcept {
        return has_flag(id, ITEM_FLAG_ENTITY_BOOK);
    }

    IO_NODISCARD static inline constexpr bool has_instance_data(Id id) noexcept {
        return has_flag(id, ITEM_FLAG_INSTANCE_DATA);
    }

    IO_NODISCARD static inline constexpr io::u16 max_stack(Id id) noexcept {
        return def(id).max_stack;
    }

    IO_NODISCARD static inline constexpr ge::voxel::BlockId place_block(Id id) noexcept {
        return def(id).place_block;
    }

    IO_NODISCARD static inline io::char_view name(Id id) noexcept {
        return def(id).name;
    }

    IO_NODISCARD static inline io::char_view short_name(Id id) noexcept {
        return def(id).short_name;
    }

    IO_NODISCARD static inline constexpr bool is_empty(const Stack& stack) noexcept {
        return stack.id == Id::None || stack.count == 0u;
    }

    static inline void normalize(Stack& stack) noexcept;
    IO_NODISCARD static inline constexpr Stack make_stack(Id id, io::u16 count, io::u16 freshness) noexcept;

    static inline void clear(Stack& stack) noexcept {
        stack = {};
    }

    static inline void apply_decay_outcome(Stack& stack) noexcept {
        if (!decays(stack.id) || stack.freshness != 0u) return;
        const Id next = def(stack.id).decay_result;
        if (!valid(next) || next == Id::None) {
            clear(stack);
            return;
        }
        const io::u16 count = stack.count;
        stack = make_stack(next, count, FRESHNESS_MAX);
        normalize(stack);
    }

    IO_NODISCARD static inline constexpr Stack make_stack(Id id, io::u16 count, io::u16 freshness = FRESHNESS_MAX) noexcept {
        return Stack{
            id,
            count,
            (decays(id) || has_instance_data(id)) ? freshness : 0u
        };
    }

    IO_NODISCARD static inline constexpr FreshnessBand freshness_band(io::u16 freshness) noexcept {
        if (freshness <= 250u) return FreshnessBand::Rotten;
        if (freshness <= 600u) return FreshnessBand::Stale;
        return FreshnessBand::Fresh;
    }

    IO_NODISCARD static inline constexpr FreshnessBand freshness_band(const Stack& stack) noexcept {
        if (!decays(stack.id)) return FreshnessBand::Fresh;
        return freshness_band(stack.freshness);
    }

    IO_NODISCARD static inline constexpr Visual resolve_visual(const Stack& stack) noexcept {
        const Def& d = def(stack.id);
        switch (freshness_band(stack)) {
        case FreshnessBand::Rotten: return d.rotten_visual;
        case FreshnessBand::Stale: return d.stale_visual;
        default: return d.fresh_visual;
        }
    }

    IO_NODISCARD static inline io::char_view visual_texture_name(Visual visual) noexcept {
        switch (visual) {
        case Visual::GrassBlock: return "grass_up";
        case Visual::DirtBlock: return "dirt";
        case Visual::StoneBlock: return "stone";
        case Visual::SandBlock: return "sand";
        case Visual::LogBlock: return "log";
        case Visual::LeavesBlock: return "leaves";
        case Visual::PotatoFresh: return "potato";
        case Visual::PotatoRotten: return "potato";
        case Visual::SpellWardBook: return "spell_ward";
        case Visual::Water: return "water";
        case Visual::Blood: return "blood";
        case Visual::Slime: return "slime";
        case Visual::Dagger: return "dagger";
        case Visual::SpellBolt: return "water";
        case Visual::SpellDig: return "blood";
        case Visual::SpellBurst: return "slime";
        case Visual::SpellBeam: return "water";
        case Visual::SpellOrb: return "slime";
        case Visual::SpellMine: return "blood";
        case Visual::SpellShieldPulse: return "snow";
        case Visual::SpellMark: return "log";
        case Visual::SpellPull: return "leaves";
        case Visual::SpellBlinkStep: return "sand";
        default: return {};
        }
    }

    IO_NODISCARD static inline constexpr bool can_stack_together(const Stack& a, const Stack& b) noexcept {
        if (is_empty(a) || is_empty(b)) return false;
        if (a.id != b.id) return false;
        if (has_instance_data(a.id) && a.freshness != b.freshness) return false;
        return true;
    }

    IO_NODISCARD static inline io::u16 blend_freshness_weighted(io::u16 f0, io::u16 c0, io::u16 f1, io::u16 c1) noexcept {
        const io::u32 w0 = c0;
        const io::u32 w1 = c1;
        const io::u32 total = w0 + w1;
        if (total == 0u) return FRESHNESS_MAX;
        const io::u32 sum = static_cast<io::u32>(f0) * w0 + static_cast<io::u32>(f1) * w1;
        const io::u32 avg = (sum + total / 2u) / total;
        return static_cast<io::u16>((avg > FRESHNESS_MAX) ? FRESHNESS_MAX : avg);
    }

    static inline void normalize(Stack& stack) noexcept {
        if (!valid(stack.id) || stack.id == Id::None || stack.count == 0u) {
            clear(stack);
            return;
        }
        const io::u16 limit = max_stack(stack.id);
        if (stack.count > limit) stack.count = limit;
        if (decays(stack.id)) {
            if (stack.freshness > FRESHNESS_MAX) stack.freshness = FRESHNESS_MAX;
            apply_decay_outcome(stack);
        } else if (has_instance_data(stack.id)) {
            // Instance token/state is carried in freshness for non-decay unique items.
        } else {
            stack.freshness = 0u;
        }
    }

    static inline void reset_player_inventory(PlayerInventory& inv) noexcept {
        inv = {};
    }

    static inline void reset_container_inventory(ContainerInventory& inv) noexcept {
        inv = {};
    }

    template<io::u32 N>
    static inline bool add_to_slots(Stack (&slots)[N], Stack& incoming) noexcept {
        normalize(incoming);
        if (is_empty(incoming)) return true;

        for (io::u32 i = 0u; i < N; ++i) {
            Stack& dst = slots[i];
            normalize(dst);
            if (!can_stack_together(dst, incoming)) continue;
            const io::u16 limit = max_stack(dst.id);
            if (dst.count >= limit) continue;
            const io::u16 dst_before = dst.count;
            io::u16 add = static_cast<io::u16>(limit - dst.count);
            if (add > incoming.count) add = incoming.count;
            dst.count = static_cast<io::u16>(dst.count + add);
            incoming.count = static_cast<io::u16>(incoming.count - add);
            if (decays(dst.id))
                dst.freshness = blend_freshness_weighted(dst.freshness, dst_before, incoming.freshness, add);
            if (incoming.count == 0u) {
                clear(incoming);
                return true;
            }
        }

        for (io::u32 i = 0u; i < N; ++i) {
            Stack& dst = slots[i];
            normalize(dst);
            if (!is_empty(dst)) continue;
            dst = incoming;
            clear(incoming);
            return true;
        }
        return false;
    }

    IO_NODISCARD static inline Stack* inventory_slots_for_category(PlayerInventory& inv, Category category) noexcept {
        switch (category) {
        case Category::Blocks: return inv.blocks;
        case Category::Consumables: return inv.consumables;
        case Category::SpellingWards: return inv.spelling_wards;
        case Category::Spells: return inv.spells;
        case Category::Materials:
        default: return inv.materials;
        }
    }

    IO_NODISCARD static inline const Stack* inventory_slots_for_category(const PlayerInventory& inv, Category category) noexcept {
        return inventory_slots_for_category(const_cast<PlayerInventory&>(inv), category);
    }

    template<io::u32 N>
    static inline bool merge_into_slots(Stack (&slots)[N], Stack& incoming) noexcept {
        normalize(incoming);
        if (is_empty(incoming)) return true;

        for (io::u32 i = 0u; i < N; ++i) {
            Stack& dst = slots[i];
            normalize(dst);
            if (!can_stack_together(dst, incoming)) continue;
            const io::u16 limit = max_stack(dst.id);
            if (dst.count >= limit) continue;
            const io::u16 dst_before = dst.count;
            io::u16 add = static_cast<io::u16>(limit - dst.count);
            if (add > incoming.count) add = incoming.count;
            dst.count = static_cast<io::u16>(dst.count + add);
            incoming.count = static_cast<io::u16>(incoming.count - add);
            if (decays(dst.id))
                dst.freshness = blend_freshness_weighted(dst.freshness, dst_before, incoming.freshness, add);
            if (incoming.count == 0u) {
                clear(incoming);
                return true;
            }
        }
        return false;
    }

    static inline bool add_to_inventory(PlayerInventory& inv, Stack incoming) noexcept {
        if (merge_into_slots(inv.hotbar, incoming)) return true;
        Stack* preferred = inventory_slots_for_category(inv, def(incoming.id).category);
        if (preferred == inv.blocks) {
            if (add_to_slots(inv.blocks, incoming)) return true;
        } else if (preferred == inv.consumables) {
            if (add_to_slots(inv.consumables, incoming)) return true;
        } else if (preferred == inv.spelling_wards) {
            if (add_to_slots(inv.spelling_wards, incoming)) return true;
        } else if (preferred == inv.spells) {
            if (add_to_slots(inv.spells, incoming)) return true;
        } else {
            if (add_to_slots(inv.materials, incoming)) return true;
        }
        if (preferred != inv.materials && add_to_slots(inv.materials, incoming)) return true;
        return add_to_slots(inv.hotbar, incoming);
    }

    IO_NODISCARD static inline Stack* slot_ptr(PlayerInventory& inv, SlotRegion region, io::u8 index_value) noexcept {
        const io::u32 index_u32 = static_cast<io::u32>(index_value);
        const io::u32 third = INVENTORY_SLOT_COUNT / 3u;
        if (region == SlotRegion::Hotbar) {
            if (index_u32 >= HOTBAR_SLOT_COUNT) return nullptr;
            return &inv.hotbar[index_u32];
        }
        if (region == SlotRegion::Blocks) {
            if (index_u32 >= INVENTORY_SLOT_COUNT) return nullptr;
            return &inv.blocks[index_u32];
        }
        if (region == SlotRegion::General) {
            if (index_u32 >= INVENTORY_SLOT_COUNT) return nullptr;
            if (index_u32 < third)
                return &inv.consumables[index_u32];
            if (index_u32 < third * 2u)
                return &inv.materials[index_u32 - third];
            return &inv.spelling_wards[index_u32 - third * 2u];
        }
        if (region == SlotRegion::Consumables) {
            if (index_u32 >= INVENTORY_SLOT_COUNT) return nullptr;
            return &inv.consumables[index_u32];
        }
        if (region == SlotRegion::Materials) {
            if (index_u32 >= INVENTORY_SLOT_COUNT) return nullptr;
            return &inv.materials[index_u32];
        }
        if (region == SlotRegion::SpellingWards) {
            if (index_u32 >= INVENTORY_SLOT_COUNT) return nullptr;
            return &inv.spelling_wards[index_u32];
        }
        if (region == SlotRegion::Spells) {
            if (index_u32 >= INVENTORY_SLOT_COUNT) return nullptr;
            return &inv.spells[index_u32];
        }
        if (region == SlotRegion::Cursor) {
            if (index_u32 != 0u) return nullptr;
            return &inv.cursor;
        }
        if (region == SlotRegion::Trash) {
            if (index_u32 != 0u) return nullptr;
            return &inv.trash;
        }
        return nullptr;
    }

    IO_NODISCARD static inline const Stack* slot_ptr(const PlayerInventory& inv, SlotRegion region, io::u8 index_value) noexcept {
        return slot_ptr(const_cast<PlayerInventory&>(inv), region, index_value);
    }

    IO_NODISCARD static inline Stack* slot_ptr(ContainerInventory& inv, io::u8 index_value) noexcept {
        const io::u32 index_u32 = static_cast<io::u32>(index_value);
        if (index_u32 >= CONTAINER_SLOT_COUNT) return nullptr;
        return &inv.slots[index_u32];
    }

    static inline bool move_between_player_slots(PlayerInventory& inv,
                                                 SlotRegion src_region, io::u8 src_index,
                                                 SlotRegion dst_region, io::u8 dst_index) noexcept {
        Stack* src = slot_ptr(inv, src_region, src_index);
        Stack* dst = slot_ptr(inv, dst_region, dst_index);
        if (!src || !dst || src == dst) return false;
        normalize(*src);
        normalize(*dst);
        if (is_empty(*src)) return false;

        if (dst_region == SlotRegion::Trash) {
            clear(*src);
            clear(*dst);
            return true;
        }

        if (is_empty(*dst)) {
            *dst = *src;
            clear(*src);
            return true;
        }

        if (can_stack_together(*src, *dst)) {
            const io::u16 limit = max_stack(dst->id);
            if (dst->count >= limit) return false;
            const io::u16 dst_before = dst->count;
            io::u16 add = static_cast<io::u16>(limit - dst->count);
            if (add > src->count) add = src->count;
            dst->count = static_cast<io::u16>(dst->count + add);
            src->count = static_cast<io::u16>(src->count - add);
            if (decays(dst->id))
                dst->freshness = blend_freshness_weighted(dst->freshness, dst_before, src->freshness, add);
            if (src->count == 0u)
                clear(*src);
            return add > 0u;
        }

        const Stack tmp = *dst;
        *dst = *src;
        *src = tmp;
        return true;
    }

    static inline bool remove_one(Stack& stack) noexcept {
        normalize(stack);
        if (is_empty(stack)) return false;
        --stack.count;
        if (stack.count == 0u)
            clear(stack);
        return true;
    }

    static inline bool left_click(PlayerInventory& inv, SlotRegion region, io::u8 index_value) noexcept {
        Stack* slot = slot_ptr(inv, region, index_value);
        if (!slot) return false;
        Stack& cursor = inv.cursor;
        normalize(*slot);
        normalize(cursor);

        if (region == SlotRegion::Trash) {
            if (!is_empty(cursor)) {
                clear(cursor);
                clear(*slot);
                return true;
            }
            if (!is_empty(*slot)) {
                clear(*slot);
                return true;
            }
            return false;
        }

        if (is_empty(cursor)) {
            if (is_empty(*slot)) return false;
            cursor = *slot;
            clear(*slot);
            return true;
        }

        if (is_empty(*slot)) {
            *slot = cursor;
            clear(cursor);
            return true;
        }

        if (can_stack_together(*slot, cursor)) {
            const io::u16 limit = max_stack(slot->id);
            if (slot->count >= limit) return false;
            const io::u16 dst_before = slot->count;
            io::u16 add = static_cast<io::u16>(limit - slot->count);
            if (add > cursor.count) add = cursor.count;
            slot->count = static_cast<io::u16>(slot->count + add);
            cursor.count = static_cast<io::u16>(cursor.count - add);
            if (decays(slot->id))
                slot->freshness = blend_freshness_weighted(slot->freshness, dst_before, cursor.freshness, add);
            if (cursor.count == 0u)
                clear(cursor);
            return add > 0u;
        }

        const Stack tmp = *slot;
        *slot = cursor;
        cursor = tmp;
        return true;
    }

    static inline bool right_click(PlayerInventory& inv, SlotRegion region, io::u8 index_value) noexcept {
        Stack* slot = slot_ptr(inv, region, index_value);
        if (!slot) return false;
        Stack& cursor = inv.cursor;
        normalize(*slot);
        normalize(cursor);

        if (region == SlotRegion::Trash) {
            if (is_empty(cursor))
                return false;
            cursor.count = static_cast<io::u16>(cursor.count - 1u);
            if (cursor.count == 0u)
                clear(cursor);
            clear(*slot);
            return true;
        }

        if (is_empty(cursor)) {
            if (is_empty(*slot)) return false;
            const io::u16 take = static_cast<io::u16>((slot->count + 1u) / 2u);
            cursor = *slot;
            cursor.count = take;
            slot->count = static_cast<io::u16>(slot->count - take);
            normalize(*slot);
            return true;
        }

        if (is_empty(*slot)) {
            *slot = make_stack(cursor.id, 1u, cursor.freshness);
            cursor.count = static_cast<io::u16>(cursor.count - 1u);
            if (cursor.count == 0u)
                clear(cursor);
            return true;
        }

        if (!can_stack_together(*slot, cursor))
            return false;

        const io::u16 limit = max_stack(slot->id);
        if (slot->count >= limit)
            return false;
        const io::u16 dst_before = slot->count;
        ++slot->count;
        if (decays(slot->id))
            slot->freshness = blend_freshness_weighted(slot->freshness, dst_before, cursor.freshness, 1u);
        cursor.count = static_cast<io::u16>(cursor.count - 1u);
        if (cursor.count == 0u)
            clear(cursor);
        return true;
    }

    static inline bool drop_from_slot(PlayerInventory& inv,
                                      SlotRegion region,
                                      io::u8 index_value,
                                      io::u16 amount,
                                      Stack& out_drop) noexcept {
        out_drop = {};
        Stack* slot = slot_ptr(inv, region, index_value);
        if (!slot) return false;
        normalize(*slot);
        if (is_empty(*slot)) return false;
        if (amount == 0u || amount > slot->count)
            amount = slot->count;
        out_drop = *slot;
        out_drop.count = amount;
        slot->count = static_cast<io::u16>(slot->count - amount);
        normalize(*slot);
        return !is_empty(out_drop);
    }

    static inline void age_stack(Stack& stack, io::u64 delta_ms) noexcept {
        normalize(stack);
        if (is_empty(stack) || !decays(stack.id) || delta_ms == 0u) return;
        const io::u32 decay_ms = def(stack.id).decay_ms;
        if (decay_ms == 0u) return;
        io::u32 delta_ms32 = 0xFFFFFFFFu;
        if (delta_ms < static_cast<io::u64>(delta_ms32))
            delta_ms32 = static_cast<io::u32>(delta_ms);

        if (delta_ms32 >= decay_ms) {
            stack.freshness = 0u;
            return;
        }

        io::u32 loss_u32 = 0u;
        if (decay_ms <= (0xFFFFFFFFu / FRESHNESS_MAX)) {
            loss_u32 = (delta_ms32 * FRESHNESS_MAX) / decay_ms;
        } else {
            const io::u32 whole = delta_ms32 / decay_ms;
            const io::u32 rem = delta_ms32 % decay_ms;
            loss_u32 = whole * FRESHNESS_MAX;
            if (loss_u32 < FRESHNESS_MAX)
                loss_u32 += (rem * FRESHNESS_MAX) / decay_ms;
        }

        const io::u16 loss = (loss_u32 > FRESHNESS_MAX) ? FRESHNESS_MAX : static_cast<io::u16>(loss_u32);
        if (loss >= stack.freshness) stack.freshness = 0u;
        else stack.freshness = static_cast<io::u16>(stack.freshness - loss);
        apply_decay_outcome(stack);
    }

    IO_NODISCARD static inline constexpr Id block_drop_for(ge::voxel::BlockId id) noexcept {
        switch (id) {
        case ge::voxel::BlockId::Grass: return Id::GrassBlock;
        case ge::voxel::BlockId::Dirt: return Id::DirtBlock;
        case ge::voxel::BlockId::Stone: return Id::StoneBlock;
        case ge::voxel::BlockId::Sand: return Id::SandBlock;
        case ge::voxel::BlockId::Log: return Id::LogBlock;
        case ge::voxel::BlockId::Leaves: return Id::LeavesBlock;
        default: return Id::None;
        }
    }

    IO_NODISCARD static inline constexpr ge::voxel::BlockId block_from_visual(Visual visual) noexcept {
        switch (visual) {
        case Visual::GrassBlock: return ge::voxel::BlockId::Grass;
        case Visual::DirtBlock: return ge::voxel::BlockId::Dirt;
        case Visual::StoneBlock: return ge::voxel::BlockId::Stone;
        case Visual::SandBlock: return ge::voxel::BlockId::Sand;
        case Visual::LogBlock: return ge::voxel::BlockId::Log;
        case Visual::LeavesBlock: return ge::voxel::BlockId::Leaves;
        case Visual::PotatoFresh: return ge::voxel::BlockId::Grass;
        case Visual::PotatoRotten: return ge::voxel::BlockId::DirtDry;
        case Visual::SpellWardBook: return ge::voxel::BlockId::LevitatingBookAnchor;
        default: return ge::voxel::BlockId::Air;
        }
    }

    IO_NODISCARD static inline constexpr bool validate_defs() noexcept {
        for (io::u32 i = 0u; i < ITEM_COUNT; ++i) {
            const Def& d = g_defs[i];
            if (d.max_stack == 0u) return false;
            if (!valid(d.decay_result)) return false;
            if ((d.flags & ITEM_FLAG_PLACEABLE) != 0u && d.place_block == ge::voxel::BlockId::Air)
                return false;
            if ((d.flags & ITEM_FLAG_CONSUMABLE) != 0u && d.hunger_gain == 0u)
                return false;
            if ((d.flags & ITEM_FLAG_DECAYS) != 0u && d.decay_ms == 0u)
                return false;
        }
        return true;
    }

    static_assert(validate_defs(), "Invalid item definition table");
} // namespace item
} // namespace ge
