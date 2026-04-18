#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"

namespace ge {
namespace voxel {
    enum class BlockId : io::u16 {
        Air = 0,
        Grass = 1,
        Dirt = 2,
        Stone = 3,
        Sand = 4,
        Water = 5,
        Blood = 6,
        Slime = 7,
        Snow = 8,
        GrassPale = 9,
        DirtDry = 10,
        StoneCracked = 11,
        SandAsh = 12,
        WaterDark = 13,
        BloodDark = 14,
        SlimeDark = 15,
        SnowDirty = 16,
        LevitatingBookAnchor = 17,
        Log = 18,
        Leaves = 19,
        __COUNT__
    };

    static constexpr io::u16 BLOCK_COUNT = static_cast<io::u16>(BlockId::__COUNT__);

    enum class RenderKind : io::u8 {
        Empty = 0,
        Cube,
        CubeFaces,
        CustomModel
    };

    enum class CollisionKind : io::u8 {
        None = 0,
        FullCube,
        CustomAabb
    };

    enum class BreakKind : io::u8 {
        Normal = 0,
        ReplaceOnBreak,
        MultiBlock
    };

    enum class PerceptionLevel : io::u8 {
        Normal = 0,
        Noise,
        Damaged,
        Exhausted,
        __COUNT__
    };

    static constexpr io::u16 PERCEPTION_LEVEL_COUNT = static_cast<io::u16>(PerceptionLevel::__COUNT__);

    enum : io::u8 {
        BLOCK_FLAG_TRANSPARENT = 1u << 0,
        BLOCK_FLAG_LIQUID      = 1u << 1,
        BLOCK_FLAG_OCCLUDER    = 1u << 2
    };

    struct BlockState {
        io::u16 id{};
        io::u16 state{};
    };

    struct BlockDef {
        RenderKind render{};
        CollisionKind collision{};
        BreakKind break_kind{};
        io::u16 replace_on_break{};
        io::u8 policy_flags{};
        io::u8 perception_group{};
    };

    IO_NODISCARD static inline constexpr io::u16 block_index(BlockId id) noexcept {
        return static_cast<io::u16>(id);
    }

    IO_NODISCARD static inline constexpr bool block_id_valid(BlockId id) noexcept {
        return block_index(id) < BLOCK_COUNT;
    }

    IO_NODISCARD static inline constexpr BlockId block_id_or_air(BlockId id) noexcept {
        return block_id_valid(id) ? id : BlockId::Air;
    }

    static constexpr BlockDef g_block_defs[BLOCK_COUNT]{
        // Air
        { RenderKind::Empty, CollisionKind::None, BreakKind::Normal,          block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT, 0u },
        // Grass
        { RenderKind::CubeFaces, CollisionKind::FullCube, BreakKind::Normal,  block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 1u },
        // Dirt
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 1u },
        // Stone
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 2u },
        // Sand
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 1u },
        // Water
        { RenderKind::Cube, CollisionKind::None, BreakKind::Normal,           block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT | BLOCK_FLAG_LIQUID, 3u },
        // Blood
        { RenderKind::Cube, CollisionKind::None, BreakKind::Normal,           block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT | BLOCK_FLAG_LIQUID, 7u },
        // Slime
        { RenderKind::Cube, CollisionKind::None, BreakKind::Normal,           block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT | BLOCK_FLAG_LIQUID, 8u },
        // Snow
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 4u },
        // GrassPale
        { RenderKind::CubeFaces, CollisionKind::FullCube, BreakKind::Normal,  block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 1u },
        // DirtDry
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 1u },
        // StoneCracked
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 2u },
        // SandAsh
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 1u },
        // WaterDark
        { RenderKind::Cube, CollisionKind::None, BreakKind::Normal,           block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT | BLOCK_FLAG_LIQUID, 3u },
        // BloodDark
        { RenderKind::Cube, CollisionKind::None, BreakKind::Normal,           block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT | BLOCK_FLAG_LIQUID, 7u },
        // SlimeDark
        { RenderKind::Cube, CollisionKind::None, BreakKind::Normal,           block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT | BLOCK_FLAG_LIQUID, 8u },
        // SnowDirty
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 4u },
        // LevitatingBookAnchor (invisible marker cell for server-driven world entity)
        { RenderKind::Empty, CollisionKind::None, BreakKind::Normal,          block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT, 0u },
        // Log
        { RenderKind::CubeFaces, CollisionKind::FullCube, BreakKind::Normal,  block_index(BlockId::Air), BLOCK_FLAG_OCCLUDER, 5u },
        // Leaves
        { RenderKind::Cube, CollisionKind::FullCube, BreakKind::Normal,       block_index(BlockId::Air), BLOCK_FLAG_TRANSPARENT, 6u },
    };

    static constexpr BlockId g_perception_remap[PERCEPTION_LEVEL_COUNT][BLOCK_COUNT]{
        // Normal
        {
            BlockId::Air, BlockId::Grass, BlockId::Dirt, BlockId::Stone, BlockId::Sand, BlockId::Water, BlockId::Blood, BlockId::Slime,
            BlockId::Snow, BlockId::GrassPale, BlockId::DirtDry, BlockId::StoneCracked, BlockId::SandAsh,
            BlockId::WaterDark, BlockId::BloodDark, BlockId::SlimeDark, BlockId::SnowDirty, BlockId::LevitatingBookAnchor, BlockId::Log, BlockId::Leaves
        },
        // Noise
        {
            BlockId::Air, BlockId::GrassPale, BlockId::Dirt, BlockId::Stone, BlockId::Sand, BlockId::Water, BlockId::Blood, BlockId::Slime,
            BlockId::Snow, BlockId::GrassPale, BlockId::DirtDry, BlockId::StoneCracked, BlockId::SandAsh,
            BlockId::WaterDark, BlockId::BloodDark, BlockId::SlimeDark, BlockId::SnowDirty, BlockId::LevitatingBookAnchor, BlockId::Log, BlockId::Leaves
        },
        // Damaged
        {
            BlockId::Air, BlockId::GrassPale, BlockId::DirtDry, BlockId::StoneCracked, BlockId::SandAsh, BlockId::WaterDark, BlockId::BloodDark, BlockId::SlimeDark,
            BlockId::SnowDirty, BlockId::GrassPale, BlockId::DirtDry, BlockId::StoneCracked, BlockId::SandAsh,
            BlockId::WaterDark, BlockId::BloodDark, BlockId::SlimeDark, BlockId::SnowDirty, BlockId::LevitatingBookAnchor, BlockId::Log, BlockId::Leaves
        },
        // Exhausted
        {
            BlockId::Air, BlockId::GrassPale, BlockId::DirtDry, BlockId::StoneCracked, BlockId::SandAsh, BlockId::WaterDark, BlockId::BloodDark, BlockId::SlimeDark,
            BlockId::SnowDirty, BlockId::GrassPale, BlockId::DirtDry, BlockId::StoneCracked, BlockId::SandAsh,
            BlockId::WaterDark, BlockId::BloodDark, BlockId::SlimeDark, BlockId::SnowDirty, BlockId::LevitatingBookAnchor, BlockId::Log, BlockId::Leaves
        }
    };

    IO_NODISCARD static inline constexpr bool simulation_compatible(const BlockDef& a, const BlockDef& b) noexcept {
        return a.render == b.render
            && a.collision == b.collision
            && a.break_kind == b.break_kind
            && a.replace_on_break == b.replace_on_break
            && a.policy_flags == b.policy_flags;
    }

    IO_NODISCARD static inline constexpr bool validate_perception_contract() noexcept {
        for (io::u16 level = 0; level < PERCEPTION_LEVEL_COUNT; ++level) {
            for (io::u16 id = 0; id < BLOCK_COUNT; ++id) {
                const BlockId remapped = g_perception_remap[level][id];
                const io::u16 remap_id = block_index(remapped);
                if (remap_id >= BLOCK_COUNT)
                    return false;
                if (!simulation_compatible(g_block_defs[id], g_block_defs[remap_id]))
                    return false;
            }
        }
        return true;
    }

    static_assert(validate_perception_contract(), "Perception remap breaks simulation contract");

    IO_NODISCARD static inline const BlockDef& block_def(BlockId id) noexcept {
        const io::u16 idx = block_index(id);
        if (idx < BLOCK_COUNT) return g_block_defs[idx];
        return g_block_defs[block_index(BlockId::Air)];
    }

    IO_NODISCARD static inline BlockId remap_perception(BlockId id, PerceptionLevel level) noexcept {
        const io::u16 id_idx = block_index(id);
        const io::u16 level_idx = static_cast<io::u16>(level);
        if (id_idx >= BLOCK_COUNT || level_idx >= PERCEPTION_LEVEL_COUNT)
            return BlockId::Air;
        return g_perception_remap[level_idx][id_idx];
    }

    IO_NODISCARD static inline bool is_transparent(BlockId id) noexcept {
        return (block_def(id).policy_flags & BLOCK_FLAG_TRANSPARENT) != 0u;
    }

    IO_NODISCARD static inline bool is_solid(BlockId id) noexcept {
        return block_def(id).collision == CollisionKind::FullCube;
    }

    IO_NODISCARD static inline bool is_occluder(BlockId id) noexcept {
        return (block_def(id).policy_flags & BLOCK_FLAG_OCCLUDER) != 0u;
    }

    IO_NODISCARD static inline bool is_liquid(BlockId id) noexcept {
        return (block_def(id).policy_flags & BLOCK_FLAG_LIQUID) != 0u;
    }
} // namespace voxel
} // namespace ge
