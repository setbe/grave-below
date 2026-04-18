#pragma once

#include "block.hpp"

namespace ge {
namespace voxel {
    enum class FluidKind : io::u8 {
        None = 0,
        Water = 1,
        Blood = 2,
        Slime = 3
    };

    struct FluidStack {
        FluidKind bottom_kind = FluidKind::None;
        io::u8 bottom_level = 0u; // 0..8
        FluidKind top_kind = FluidKind::None;
        io::u8 top_level = 0u;    // 0..8
    };

    static constexpr io::u8 FLUID_LEVEL_MAX = 8u;

    // Packed state versioning:
    // - V1 (legacy): kinds on 2 bits each.
    // - V2 (current): kinds on 3 bits each + marker flag (bit 14).
    static constexpr io::u16 FLUID_PACK_V2_FLAG = 0x4000u;      // bit 14
    static constexpr io::u16 FLUID_BOTTOM_LEVEL_MASK_V1 = 0x000Fu; // bits 0..3
    static constexpr io::u16 FLUID_BOTTOM_KIND_MASK_V1 = 0x0030u;  // bits 4..5
    static constexpr io::u16 FLUID_TOP_LEVEL_MASK_V1 = 0x03C0u;    // bits 6..9
    static constexpr io::u16 FLUID_TOP_KIND_MASK_V1 = 0x0C00u;     // bits 10..11

    static constexpr io::u16 FLUID_BOTTOM_LEVEL_MASK = 0x000Fu; // bits 0..3
    static constexpr io::u16 FLUID_BOTTOM_KIND_MASK = 0x0070u;  // bits 4..6 (3-bit kind)
    static constexpr io::u16 FLUID_TOP_LEVEL_MASK = 0x0780u;    // bits 7..10
    static constexpr io::u16 FLUID_TOP_KIND_MASK = 0x3800u;     // bits 11..13 (3-bit kind)

    IO_NODISCARD static inline io::u8 clamp_fluid_level(io::u8 level) noexcept {
        if (level > FLUID_LEVEL_MAX) return FLUID_LEVEL_MAX;
        return level;
    }

    IO_NODISCARD static inline FluidKind fluid_kind_from_block_id(BlockId id) noexcept {
        switch (id) {
        case BlockId::Water:
        case BlockId::WaterDark: return FluidKind::Water;
        case BlockId::Blood:
        case BlockId::BloodDark: return FluidKind::Blood;
        case BlockId::Slime:
        case BlockId::SlimeDark: return FluidKind::Slime;
        default: return FluidKind::None;
        }
    }

    IO_NODISCARD static inline bool is_fluid_block_id(BlockId id) noexcept {
        return fluid_kind_from_block_id(id) != FluidKind::None;
    }

    IO_NODISCARD static inline BlockId fluid_dark_block_id(FluidKind kind) noexcept {
        switch (kind) {
        case FluidKind::Water: return BlockId::WaterDark;
        case FluidKind::Blood: return BlockId::BloodDark;
        case FluidKind::Slime: return BlockId::SlimeDark;
        default: return BlockId::Air;
        }
    }

    IO_NODISCARD static inline constexpr io::u8 fluid_density(FluidKind kind) noexcept {
        // Higher value means "sinks deeper".
        switch (kind) {
        case FluidKind::Water: return 3u;
        case FluidKind::Blood: return 4u;
        case FluidKind::Slime: return 2u;
        default: return 0u;
        }
    }

    static inline void normalize_fluid_stack(FluidStack& s) noexcept {
        s.bottom_level = clamp_fluid_level(s.bottom_level);
        s.top_level = clamp_fluid_level(s.top_level);

        if (s.bottom_kind == FluidKind::None) s.bottom_level = 0u;
        if (s.top_kind == FluidKind::None) s.top_level = 0u;

        if (s.bottom_level == 0u) s.bottom_kind = FluidKind::None;
        if (s.top_level == 0u) s.top_kind = FluidKind::None;

        if (s.bottom_kind == FluidKind::None && s.top_kind != FluidKind::None) {
            s.bottom_kind = s.top_kind;
            s.bottom_level = s.top_level;
            s.top_kind = FluidKind::None;
            s.top_level = 0u;
        }

        if (s.bottom_kind != FluidKind::None && s.top_kind != FluidKind::None &&
            s.bottom_kind == s.top_kind) {
            io::u16 merged = static_cast<io::u16>(s.bottom_level + s.top_level);
            if (merged > FLUID_LEVEL_MAX) merged = FLUID_LEVEL_MAX;
            s.bottom_level = static_cast<io::u8>(merged);
            s.top_kind = FluidKind::None;
            s.top_level = 0u;
        }

        if (s.bottom_kind != FluidKind::None && s.top_kind != FluidKind::None) {
            if (fluid_density(s.top_kind) > fluid_density(s.bottom_kind)) {
                const FluidKind k = s.bottom_kind;
                const io::u8 l = s.bottom_level;
                s.bottom_kind = s.top_kind;
                s.bottom_level = s.top_level;
                s.top_kind = k;
                s.top_level = l;
            }
        }

        io::u16 total = static_cast<io::u16>(s.bottom_level + s.top_level);
        if (total > FLUID_LEVEL_MAX) {
            io::u16 overflow = static_cast<io::u16>(total - FLUID_LEVEL_MAX);
            if (overflow >= s.bottom_level) {
                overflow = static_cast<io::u16>(overflow - s.bottom_level);
                s.bottom_level = 0u;
                s.bottom_kind = FluidKind::None;
                if (overflow >= s.top_level) {
                    s.top_level = 0u;
                    s.top_kind = FluidKind::None;
                } else {
                    s.top_level = static_cast<io::u8>(s.top_level - overflow);
                }
            } else {
                s.bottom_level = static_cast<io::u8>(s.bottom_level - overflow);
            }
        }

        if (s.bottom_level == 0u) s.bottom_kind = FluidKind::None;
        if (s.top_level == 0u) s.top_kind = FluidKind::None;
        if (s.bottom_kind == FluidKind::None && s.top_kind != FluidKind::None) {
            s.bottom_kind = s.top_kind;
            s.bottom_level = s.top_level;
            s.top_kind = FluidKind::None;
            s.top_level = 0u;
        }
    }

    IO_NODISCARD static inline FluidStack unpack_fluid_stack_state_v1(io::u16 state) noexcept {
        FluidStack s{};
        s.bottom_level = static_cast<io::u8>(state & FLUID_BOTTOM_LEVEL_MASK_V1);
        s.bottom_kind = static_cast<FluidKind>((state >> 4u) & 0x03u);
        s.top_level = static_cast<io::u8>((state >> 6u) & 0x0Fu);
        s.top_kind = static_cast<FluidKind>((state >> 10u) & 0x03u);
        normalize_fluid_stack(s);
        return s;
    }

    IO_NODISCARD static inline io::u16 pack_fluid_stack_state(FluidStack s) noexcept {
        normalize_fluid_stack(s);
        if (s.bottom_kind == FluidKind::None)
            return 0u;
        io::u16 out = 0u;
        out |= static_cast<io::u16>(s.bottom_level & 0x0Fu);
        out |= static_cast<io::u16>((static_cast<io::u16>(s.bottom_kind) & 0x07u) << 4u);
        out |= static_cast<io::u16>((static_cast<io::u16>(s.top_level) & 0x0Fu) << 7u);
        out |= static_cast<io::u16>((static_cast<io::u16>(s.top_kind) & 0x07u) << 11u);
        out |= FLUID_PACK_V2_FLAG;
        return out;
    }

    IO_NODISCARD static inline FluidStack unpack_fluid_stack_state(io::u16 state) noexcept {
        if ((state & FLUID_PACK_V2_FLAG) == 0u)
            return unpack_fluid_stack_state_v1(state);

        FluidStack s{};
        s.bottom_level = static_cast<io::u8>(state & 0x0Fu);
        s.bottom_kind = static_cast<FluidKind>((state >> 4u) & 0x07u);
        s.top_level = static_cast<io::u8>((state >> 7u) & 0x0Fu);
        s.top_kind = static_cast<FluidKind>((state >> 11u) & 0x07u);
        normalize_fluid_stack(s);
        return s;
    }

    IO_NODISCARD static inline FluidStack fluid_stack_from_block(BlockId id, io::u16 state) noexcept {
        FluidStack s = unpack_fluid_stack_state(state);

        const bool packed_has_kinds =
            ((state & FLUID_PACK_V2_FLAG) != 0u)
            ? (((state & FLUID_BOTTOM_KIND_MASK) != 0u) || ((state & FLUID_TOP_KIND_MASK) != 0u))
            : (((state & FLUID_BOTTOM_KIND_MASK_V1) != 0u) || ((state & FLUID_TOP_KIND_MASK_V1) != 0u));
        if (!packed_has_kinds) {
            const FluidKind legacy_kind = fluid_kind_from_block_id(id);
            if (legacy_kind != FluidKind::None) {
                io::u8 level = static_cast<io::u8>(state & 0x0Fu);
                if (id == BlockId::Water || id == BlockId::Blood || id == BlockId::Slime)
                    level = FLUID_LEVEL_MAX;
                if (level == 0u) level = FLUID_LEVEL_MAX;
                s.bottom_kind = legacy_kind;
                s.bottom_level = clamp_fluid_level(level);
                s.top_kind = FluidKind::None;
                s.top_level = 0u;
            } else {
                s = {};
            }
        }

        normalize_fluid_stack(s);
        return s;
    }

    static inline void fluid_stack_to_block(FluidStack s, BlockId& out_id, io::u16& out_state) noexcept {
        normalize_fluid_stack(s);
        if (s.bottom_kind == FluidKind::None) {
            out_id = BlockId::Air;
            out_state = 0u;
            return;
        }
        const FluidKind dominant = (s.top_kind != FluidKind::None && s.top_level > 0u) ? s.top_kind : s.bottom_kind;
        out_id = fluid_dark_block_id(dominant);
        out_state = pack_fluid_stack_state(s);
    }

    IO_NODISCARD static inline io::u8 fluid_total_level(const FluidStack& s) noexcept {
        io::u16 total = static_cast<io::u16>(s.bottom_level + s.top_level);
        if (total > FLUID_LEVEL_MAX) total = FLUID_LEVEL_MAX;
        return static_cast<io::u8>(total);
    }

    IO_NODISCARD static inline io::u8 fluid_total_level_from_block(BlockId id, io::u16 state) noexcept {
        const FluidStack s = fluid_stack_from_block(id, state);
        return fluid_total_level(s);
    }
} // namespace voxel
} // namespace ge
