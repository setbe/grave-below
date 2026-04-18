#pragma once

#include "../../../engine/core/config.hpp"

namespace ge {
namespace region {
    using RegionId = io::u64;

    static constexpr io::u32 VALUE_MAX = 65535u;
    static constexpr io::i32 VORONOI_CELL_SIZE = 384;
    static constexpr io::i32 VERTICAL_BAND_SIZE = 512;
    static constexpr io::u32 NEIGHBOR_COUNT = 8u;
    static constexpr io::u32 CLIENT_SYNC_MAX_REGIONS = 12u;

    struct Coord {
        io::i32 cell_x = 0;
        io::i32 cell_z = 0;
        io::i32 band_y = 0;
    };

    enum class ManaBand : io::u8 {
        Exhausted = 0u,
        Critical = 1u,
        Low = 2u,
        Healthy = 3u,
        Full = 4u
    };

    enum class InstabilityBand : io::u8 {
        Stable = 0u,
        Shifting = 1u,
        Unstable = 2u,
        Chaotic = 3u
    };

    enum class DecayBand : io::u8 {
        Alive = 0u,
        Faded = 1u,
        Deadened = 2u,
        Ruined = 3u
    };

    IO_NODISCARD static inline io::i32 abs_i32(io::i32 v) noexcept {
        return v < 0 ? -v : v;
    }

    IO_NODISCARD static inline io::i32 floor_div_i32(io::i32 a, io::i32 b) noexcept {
        if (b == 0) return 0;
        io::i32 q = a / b;
        io::i32 r = a % b;
        if ((r != 0) && ((r < 0) != (b < 0))) --q;
        return q;
    }

    IO_NODISCARD static inline io::u32 hash_u32(io::u32 x) noexcept {
        x ^= x >> 16u;
        x *= 0x7FEB352Du;
        x ^= x >> 15u;
        x *= 0x846CA68Bu;
        x ^= x >> 16u;
        return x;
    }

    IO_NODISCARD static inline io::u32 hash_coords(io::i32 x, io::i32 z, io::u32 salt) noexcept {
        io::u32 h = hash_u32(static_cast<io::u32>(x) ^ 0x9E3779B9u);
        h ^= hash_u32(static_cast<io::u32>(z) ^ 0x85EBCA6Bu);
        h ^= hash_u32(salt ^ 0xC2B2AE35u);
        return hash_u32(h);
    }

    IO_NODISCARD static inline io::u32 hash_region_id(RegionId id) noexcept {
        const io::u32 lo = static_cast<io::u32>(id & 0xFFFFFFFFull);
        const io::u32 hi = static_cast<io::u32>((id >> 32u) & 0xFFFFFFFFull);
        io::u32 h = hash_u32(lo ^ 0x9E3779B9u);
        h ^= hash_u32(hi ^ 0x85EBCA6Bu);
        return hash_u32(h);
    }

    IO_NODISCARD static inline io::i32 clamp_i32(io::i32 value, io::i32 lo, io::i32 hi) noexcept {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
    }

    IO_NODISCARD static inline io::u16 clamp_u16_delta(io::u16 base, io::i32 delta) noexcept {
        io::i32 v = static_cast<io::i32>(base) + delta;
        v = clamp_i32(v, 0, static_cast<io::i32>(VALUE_MAX));
        return static_cast<io::u16>(v);
    }

    IO_NODISCARD static inline float to_norm(io::u16 value) noexcept {
        return static_cast<float>(value) / static_cast<float>(VALUE_MAX);
    }

    IO_NODISCARD static inline io::u16 from_norm(float value) noexcept {
        if (!(value == value) || value <= 0.f) return 0u;
        if (value >= 1.f) return static_cast<io::u16>(VALUE_MAX);
        return static_cast<io::u16>(value * static_cast<float>(VALUE_MAX) + 0.5f);
    }

    IO_NODISCARD static inline io::u8 packed_bands(ManaBand mana, InstabilityBand instability, DecayBand decay) noexcept {
        const io::u8 m = static_cast<io::u8>(mana) & 0x07u;
        const io::u8 i = static_cast<io::u8>(instability) & 0x03u;
        const io::u8 d = static_cast<io::u8>(decay) & 0x03u;
        return static_cast<io::u8>(m | (i << 3u) | (d << 5u));
    }

    IO_NODISCARD static inline ManaBand mana_band(io::u16 v) noexcept {
        const float n = to_norm(v);
        if (n >= 0.92f) return ManaBand::Full;
        if (n >= 0.64f) return ManaBand::Healthy;
        if (n >= 0.38f) return ManaBand::Low;
        if (n >= 0.16f) return ManaBand::Critical;
        return ManaBand::Exhausted;
    }

    IO_NODISCARD static inline InstabilityBand instability_band(io::u16 v) noexcept {
        const float n = to_norm(v);
        if (n < 0.22f) return InstabilityBand::Stable;
        if (n < 0.46f) return InstabilityBand::Shifting;
        if (n < 0.74f) return InstabilityBand::Unstable;
        return InstabilityBand::Chaotic;
    }

    IO_NODISCARD static inline DecayBand decay_band(io::u16 v) noexcept {
        const float n = to_norm(v);
        if (n < 0.18f) return DecayBand::Alive;
        if (n < 0.40f) return DecayBand::Faded;
        if (n < 0.70f) return DecayBand::Deadened;
        return DecayBand::Ruined;
    }

    IO_NODISCARD static inline io::i32 vertical_band_for_world_y(io::i32 wy) noexcept {
        return floor_div_i32(wy, VERTICAL_BAND_SIZE);
    }

    IO_NODISCARD static inline RegionId pack_region_id(const Coord& c) noexcept {
        static constexpr io::i32 BIAS = (1 << 20);
        const io::u64 x = static_cast<io::u64>(static_cast<io::u32>(c.cell_x + BIAS) & 0x1FFFFFu);
        const io::u64 z = static_cast<io::u64>(static_cast<io::u32>(c.cell_z + BIAS) & 0x1FFFFFu);
        const io::u64 y = static_cast<io::u64>(static_cast<io::u32>(c.band_y + BIAS) & 0x1FFFFFu);
        return (y << 42u) | (x << 21u) | z;
    }

    IO_NODISCARD static inline Coord unpack_region_id(RegionId id) noexcept {
        static constexpr io::i32 BIAS = (1 << 20);
        Coord out{};
        const io::u32 z = static_cast<io::u32>(id & 0x1FFFFFull);
        const io::u32 x = static_cast<io::u32>((id >> 21u) & 0x1FFFFFull);
        const io::u32 y = static_cast<io::u32>((id >> 42u) & 0x1FFFFFull);
        out.cell_x = static_cast<io::i32>(x) - BIAS;
        out.cell_z = static_cast<io::i32>(z) - BIAS;
        out.band_y = static_cast<io::i32>(y) - BIAS;
        return out;
    }

    IO_NODISCARD static inline Coord neighbor_coord(const Coord& c, io::u32 n) noexcept {
        static constexpr io::i32 OFF[NEIGHBOR_COUNT][2]{
            { -1,  0 }, {  1,  0 }, { 0, -1 }, { 0, 1 },
            { -1, -1 }, { -1,  1 }, { 1, -1 }, { 1, 1 }
        };
        Coord out = c;
        const io::u32 k = n % NEIGHBOR_COUNT;
        out.cell_x += OFF[k][0];
        out.cell_z += OFF[k][1];
        return out;
    }

    static inline void voronoi_seed_point(io::i32 cell_x, io::i32 cell_z, float& out_x, float& out_z) noexcept {
        const io::u32 h = hash_coords(cell_x, cell_z, 0xBADC0DEu);
        const io::i32 cell_x0 = cell_x * VORONOI_CELL_SIZE;
        const io::i32 cell_z0 = cell_z * VORONOI_CELL_SIZE;
        const io::i32 jitter_span = VORONOI_CELL_SIZE / 2;
        const io::i32 jx = static_cast<io::i32>((h >> 4u) % static_cast<io::u32>(jitter_span)) - jitter_span / 2;
        const io::i32 jz = static_cast<io::i32>((h >> 13u) % static_cast<io::u32>(jitter_span)) - jitter_span / 2;
        out_x = static_cast<float>(cell_x0 + VORONOI_CELL_SIZE / 2 + jx);
        out_z = static_cast<float>(cell_z0 + VORONOI_CELL_SIZE / 2 + jz);
    }

    IO_NODISCARD static inline Coord coord_from_world(io::i32 wx, io::i32 wy, io::i32 wz) noexcept {
        const io::i32 base_x = floor_div_i32(wx, VORONOI_CELL_SIZE);
        const io::i32 base_z = floor_div_i32(wz, VORONOI_CELL_SIZE);
        io::i32 best_x = base_x;
        io::i32 best_z = base_z;
        io::i64 best_d2 = static_cast<io::i64>(0x7FFFFFFF);
        for (io::i32 dz = -1; dz <= 1; ++dz) {
            for (io::i32 dx = -1; dx <= 1; ++dx) {
                const io::i32 cx = base_x + dx;
                const io::i32 cz = base_z + dz;
                float sx = 0.f;
                float sz = 0.f;
                voronoi_seed_point(cx, cz, sx, sz);
                const io::i32 ddx = wx - static_cast<io::i32>(sx);
                const io::i32 ddz = wz - static_cast<io::i32>(sz);
                const io::i64 d2 = static_cast<io::i64>(ddx) * static_cast<io::i64>(ddx) +
                                   static_cast<io::i64>(ddz) * static_cast<io::i64>(ddz);
                if (d2 < best_d2) {
                    best_d2 = d2;
                    best_x = cx;
                    best_z = cz;
                }
            }
        }
        Coord out{};
        out.cell_x = best_x;
        out.cell_z = best_z;
        out.band_y = vertical_band_for_world_y(wy);
        return out;
    }

    IO_NODISCARD static inline RegionId region_id_from_world(io::i32 wx, io::i32 wy, io::i32 wz) noexcept {
        return pack_region_id(coord_from_world(wx, wy, wz));
    }

    // Distance (in world blocks) from a point to the closest Voronoi border in XZ plane.
    // This uses the nearest and second-nearest seed pair, which gives an exact distance
    // to their bisector and a stable "how close to border" metric for debug/perception.
    IO_NODISCARD static inline float border_distance_from_world_xz(io::i32 wx, io::i32 wz) noexcept {
        const io::i32 base_x = floor_div_i32(wx, VORONOI_CELL_SIZE);
        const io::i32 base_z = floor_div_i32(wz, VORONOI_CELL_SIZE);

        float best_sx = 0.f, best_sz = 0.f;
        float second_sx = 0.f, second_sz = 0.f;
        io::i64 best_d2 = static_cast<io::i64>(0x7FFFFFFFFFFFFFFFll);
        io::i64 second_d2 = static_cast<io::i64>(0x7FFFFFFFFFFFFFFFll);
        bool have_best = false;
        bool have_second = false;

        for (io::i32 dz = -2; dz <= 2; ++dz) {
            for (io::i32 dx = -2; dx <= 2; ++dx) {
                const io::i32 cx = base_x + dx;
                const io::i32 cz = base_z + dz;
                float sx = 0.f;
                float sz = 0.f;
                voronoi_seed_point(cx, cz, sx, sz);
                const io::i32 ddx = wx - static_cast<io::i32>(sx);
                const io::i32 ddz = wz - static_cast<io::i32>(sz);
                const io::i64 d2 = static_cast<io::i64>(ddx) * static_cast<io::i64>(ddx) +
                                   static_cast<io::i64>(ddz) * static_cast<io::i64>(ddz);

                if (!have_best || d2 < best_d2) {
                    if (have_best) {
                        second_d2 = best_d2;
                        second_sx = best_sx;
                        second_sz = best_sz;
                        have_second = true;
                    }
                    best_d2 = d2;
                    best_sx = sx;
                    best_sz = sz;
                    have_best = true;
                } else if (!have_second || d2 < second_d2) {
                    second_d2 = d2;
                    second_sx = sx;
                    second_sz = sz;
                    have_second = true;
                }
            }
        }

        if (!have_best || !have_second)
            return static_cast<float>(VORONOI_CELL_SIZE) * 0.5f;

        const float vx = second_sx - best_sx;
        const float vz = second_sz - best_sz;
        const float len2 = vx * vx + vz * vz;
        if (len2 <= 0.0001f)
            return static_cast<float>(VORONOI_CELL_SIZE) * 0.5f;

        const float px = static_cast<float>(wx);
        const float pz = static_cast<float>(wz);
        const float dx1 = px - best_sx;
        const float dz1 = pz - best_sz;
        const float dx2 = px - second_sx;
        const float dz2 = pz - second_sz;
        const float d2_1 = dx1 * dx1 + dz1 * dz1;
        const float d2_2 = dx2 * dx2 + dz2 * dz2;
        float root = len2;
        // Newton-Raphson sqrt (freestanding-safe, no CRT math dependency).
        if (root > 0.f) {
            float r = (root > 1.f) ? (root * 0.5f + 1.f) : 1.f;
            for (io::u32 i = 0u; i < 6u; ++i)
                r = 0.5f * (r + root / r);
            root = r;
        } else {
            root = 0.f;
        }
        if (root <= 0.00001f)
            return static_cast<float>(VORONOI_CELL_SIZE) * 0.5f;

        const float inv = 1.0f / (2.0f * root);
        float diff = d2_2 - d2_1;
        if (diff < 0.f) diff = -diff;
        float dist = diff * inv;
        if (!(dist == dist) || dist < 0.f) dist = 0.f;
        return dist;
    }

    static inline void default_state_values(RegionId id, io::u16& out_mana, io::u16& out_instability, io::u16& out_decay, io::u8& out_type) noexcept {
        const io::u32 h0 = hash_region_id(id);
        const io::u32 h1 = hash_u32(h0 ^ 0xA55AA55Au);
        out_type = static_cast<io::u8>(h0 & 0x03u);

        io::u16 mana = static_cast<io::u16>(56000u + (h0 % 8000u));          // ~0.854..0.976
        io::u16 instability = static_cast<io::u16>((h1 >> 7u) % 6500u);      // ~0.0..0.099
        io::u16 decay = static_cast<io::u16>((h1 >> 15u) % 3200u);           // ~0.0..0.049

        // Rare region archetypes for non-uniform baseline.
        if ((h0 & 0x1Fu) == 0u) {
            decay = clamp_u16_delta(decay, 2600);
            mana = clamp_u16_delta(mana, -2400);
            out_type = 2u;
        }
        if (((h1 >> 5u) & 0x3Fu) == 7u) {
            instability = clamp_u16_delta(instability, 4200);
            out_type = 1u;
        }

        out_mana = mana;
        out_instability = instability;
        out_decay = decay;
    }
} // namespace region
} // namespace ge
