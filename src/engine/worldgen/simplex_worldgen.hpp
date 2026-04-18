#pragma once

#include "../voxel/chunk.hpp"
#include "../../../3rd_party/SimplexNoise/SimplexNoise.hpp"

namespace ge {
namespace worldgen {
    struct SimplexTerrain {
        siv::SimplexNoise noise{};
        io::i32 sea_level = 22;
        io::i32 base_level = 34;
        io::i32 min_world_y = -96;
        io::i32 max_world_y = 192;

        explicit SimplexTerrain(io::u32 seed = 1337u) noexcept {
            noise.reseed(seed);
        }

        IO_NODISCARD static inline float clamp01(float value) noexcept {
            if (value < 0.f) return 0.f;
            if (value > 1.f) return 1.f;
            return value;
        }

        IO_NODISCARD static inline io::i32 floor_to_i32(float value) noexcept {
            io::i32 out = static_cast<io::i32>(value);
            if (static_cast<float>(out) > value) --out;
            return out;
        }

        IO_NODISCARD static inline io::i32 abs_i32(io::i32 value) noexcept {
            return value < 0 ? -value : value;
        }

        IO_NODISCARD static inline io::u32 hash_u32(io::u32 value) noexcept {
            value ^= value >> 16;
            value *= 0x7FEB352Du;
            value ^= value >> 15;
            value *= 0x846CA68Bu;
            value ^= value >> 16;
            return value;
        }

        IO_NODISCARD static inline io::u32 hash_coords(io::i32 x, io::i32 z, io::u32 salt) noexcept {
            io::u32 h = hash_u32(static_cast<io::u32>(x) ^ 0x9E3779B9u);
            h ^= hash_u32(static_cast<io::u32>(z) ^ 0x85EBCA6Bu);
            h ^= hash_u32(salt ^ 0xC2B2AE35u);
            return hash_u32(h);
        }

        struct TreeBlock {
            io::i32 x = 0;
            io::i32 y = 0;
            io::i32 z = 0;
            voxel::BlockId id = voxel::BlockId::Air;
        };

        // Hard cap for generated tree layout. Must stay below 300 by design.
        static constexpr io::u32 TREE_LAYOUT_MAX_BLOCKS = 299u;
        // Max horizontal spread from tree anchor used for candidate checks and chunk stamping overlap.
        static constexpr io::i32 TREE_MAX_RADIUS_XZ = 10;

        IO_NODISCARD inline float macro_shape(io::i32 gx, io::i32 gz) const noexcept {
            const float x = static_cast<float>(gx);
            const float z = static_cast<float>(gz);
            const float raw = noise.domainWarpOctave2D(x * 0.00145f, z * 0.00145f,
                                                        5, 0.52f, 2.0f, 1.00f, 0.70f);
            const float denom = siv::SimplexNoise::maxAmp(5, 0.52f);
            return (denom > 0.f) ? (raw / denom) : 0.f;
        }

        IO_NODISCARD inline float mountain_ridges(io::i32 gx, io::i32 gz) const noexcept {
            const float x = static_cast<float>(gx);
            const float z = static_cast<float>(gz);
            return noise.domainWarpRidged2D(x * 0.00295f, z * 0.00295f,
                                            6, 0.86f, 0.72f, 2.0f, 2.0f, 1.0f);
        }

        IO_NODISCARD inline float detail_shape(io::i32 gx, io::i32 gz) const noexcept {
            const float x = static_cast<float>(gx);
            const float z = static_cast<float>(gz);
            return noise.normalizedOctave2D(x * 0.0155f, z * 0.0155f, 3, 0.56f, 2.0f);
        }

        IO_NODISCARD inline io::i32 surface_height_at(io::i32 gx, io::i32 gz) const noexcept {
            const float macro = macro_shape(gx, gz);
            const float ridges = mountain_ridges(gx, gz);
            const float base = noise.normalizedOctave2D(static_cast<float>(gx) * 0.0056f,
                                                        static_cast<float>(gz) * 0.0056f,
                                                        6, 0.5f, 2.0f);
            const float detail = detail_shape(gx, gz);
            const float plateau = noise.ridged2D(static_cast<float>(gx) * 0.00105f,
                                                 static_cast<float>(gz) * 0.00105f,
                                                 3, 2.0f, 2.0f, 1.0f);

            const float mountain_mask = clamp01((macro + 1.0f) * 0.56f);
            const float valley_mask = clamp01((0.29f - macro) * 1.48f);

            float h = static_cast<float>(base_level);
            h += base * 15.0f;
            h += (ridges * 41.0f - 6.0f) * mountain_mask;
            h += detail * 4.4f;
            h += plateau * 5.2f;
            h -= valley_mask * 12.5f;

            io::i32 hi = floor_to_i32(h);
            if (hi < min_world_y + 8) hi = min_world_y + 8;
            if (hi > max_world_y - 8) hi = max_world_y - 8;
            return hi;
        }

        IO_NODISCARD inline bool world_to_local_if_inside_chunk(const voxel::ChunkData& out,
                                                                io::i32 wx, io::i32 wy, io::i32 wz,
                                                                io::u32& lx, io::u32& ly, io::u32& lz) const noexcept {
            const io::i64 chunk_x0 = static_cast<io::i64>(out.coord.x) * static_cast<io::i64>(voxel::CHUNK_W);
            const io::i64 chunk_y0 = static_cast<io::i64>(out.coord.y) * static_cast<io::i64>(voxel::CHUNK_H);
            const io::i64 chunk_z0 = static_cast<io::i64>(out.coord.z) * static_cast<io::i64>(voxel::CHUNK_D);
            const io::i64 chunk_x1 = chunk_x0 + static_cast<io::i64>(voxel::CHUNK_W);
            const io::i64 chunk_y1 = chunk_y0 + static_cast<io::i64>(voxel::CHUNK_H);
            const io::i64 chunk_z1 = chunk_z0 + static_cast<io::i64>(voxel::CHUNK_D);

            const io::i64 wx64 = static_cast<io::i64>(wx);
            const io::i64 wy64 = static_cast<io::i64>(wy);
            const io::i64 wz64 = static_cast<io::i64>(wz);
            if (wx64 < chunk_x0 || wx64 >= chunk_x1) return false;
            if (wy64 < chunk_y0 || wy64 >= chunk_y1) return false;
            if (wz64 < chunk_z0 || wz64 >= chunk_z1) return false;

            lx = static_cast<io::u32>(wx64 - chunk_x0);
            ly = static_cast<io::u32>(wy64 - chunk_y0);
            lz = static_cast<io::u32>(wz64 - chunk_z0);
            return true;
        }

        IO_NODISCARD inline bool tree_anchor_candidate(io::i32 anchor_cx, io::i32 anchor_cz, io::u32 attempt,
                                                       io::i32& out_gx, io::i32& out_gz, io::u32& out_seed) const noexcept {
            const io::i32 anchor_x0 = anchor_cx * static_cast<io::i32>(voxel::CHUNK_W);
            const io::i32 anchor_z0 = anchor_cz * static_cast<io::i32>(voxel::CHUNK_D);
            const io::u32 salt = attempt * 0x85EBCA6Bu;
            const io::u32 h = hash_coords(anchor_x0, anchor_z0, salt);
            if ((h & 3u) != 0u)
                return false;
            const io::u32 span_x = voxel::CHUNK_W - 6u;
            const io::u32 span_z = voxel::CHUNK_D - 6u;
            const io::i32 lx = 3 + static_cast<io::i32>((h >> 4) % span_x);
            const io::i32 lz = 3 + static_cast<io::i32>((h >> 13) % span_z);
            out_gx = anchor_x0 + lx;
            out_gz = anchor_z0 + lz;
            out_seed = h;
            return true;
        }

        IO_NODISCARD static inline bool tree_append_block(TreeBlock* out_blocks, io::u32 out_cap, io::u32& out_count,
                                                          io::i32 x, io::i32 y, io::i32 z, voxel::BlockId id) noexcept {
            if (!out_blocks || out_cap == 0u) return false;
            if (id == voxel::BlockId::Air) return true;
            for (io::u32 i = 0u; i < out_count; ++i) {
                TreeBlock& b = out_blocks[i];
                if (b.x != x || b.y != y || b.z != z) continue;
                // Log always wins against leaves to keep branch/trunk continuity.
                if (id == voxel::BlockId::Log && b.id != voxel::BlockId::Log)
                    b.id = voxel::BlockId::Log;
                return true;
            }
            if (out_count >= out_cap) return false;
            out_blocks[out_count++] = TreeBlock{ x, y, z, id };
            return true;
        }

        inline void tree_emit_leaf_disc(TreeBlock* out_blocks, io::u32 out_cap, io::u32& out_count,
                                        io::i32 cx, io::i32 cy, io::i32 cz, io::i32 radius, io::u32 seed) const noexcept {
            if (out_count >= out_cap) return;
            if (radius <= 0) {
                (void)tree_append_block(out_blocks, out_cap, out_count, cx, cy, cz, voxel::BlockId::Leaves);
                return;
            }
            for (io::i32 dz = -radius; dz <= radius; ++dz) {
                for (io::i32 dx = -radius; dx <= radius; ++dx) {
                    if (out_count >= out_cap) return;
                    const io::i32 ax = abs_i32(dx);
                    const io::i32 az = abs_i32(dz);
                    if (ax + az > radius + 1) continue;
                    // Soft stochastic trim on extreme corners for organic silhouettes.
                    if (ax == radius && az == radius) {
                        const io::u32 h = hash_coords(cx + dx, cz + dz, seed ^ static_cast<io::u32>(cy * 131));
                        if ((h & 1u) != 0u) continue;
                    }
                    (void)tree_append_block(out_blocks, out_cap, out_count, cx + dx, cy, cz + dz, voxel::BlockId::Leaves);
                }
            }
        }

        inline void tree_emit_leaf_blob(TreeBlock* out_blocks, io::u32 out_cap, io::u32& out_count,
                                        io::i32 cx, io::i32 cy, io::i32 cz, io::i32 radius, io::u32 seed) const noexcept {
            if (out_count >= out_cap) return;
            for (io::i32 dy = -radius; dy <= radius; ++dy) {
                if (out_count >= out_cap) return;
                io::i32 disc_r = radius;
                if (abs_i32(dy) == radius) disc_r = radius - 1;
                if (disc_r < 0) disc_r = 0;
                tree_emit_leaf_disc(out_blocks, out_cap, out_count, cx, cy + dy, cz, disc_r,
                                    seed ^ static_cast<io::u32>((dy + 7) * 8191));
            }
        }

        IO_NODISCARD inline bool build_tree_layout(io::i32 gx, io::i32 gz, io::u32 tree_seed,
                                                   TreeBlock* out_blocks, io::u32 out_cap,
                                                   io::u32& out_count, bool& out_is_large) const noexcept {
            out_count = 0u;
            out_is_large = false;
            if (!out_blocks || out_cap == 0u) return false;

            const io::i32 surface = surface_height_at(gx, gz);
            if (surface <= sea_level + 2) return false;
            if (surface >= 102) return false;

            const bool can_be_large = surface >= sea_level + 8 && surface < 94;
            const bool large = can_be_large && (((tree_seed >> 5u) & 1u) == 0u);
            out_is_large = large;

            const io::i32 trunk_w = (large && ((tree_seed >> 9u) & 3u) != 0u) ? 2 : 1;
            io::i32 trunk_h = large
                ? (10 + static_cast<io::i32>((tree_seed >> 14u) & 3u) + static_cast<io::i32>((tree_seed >> 23u) & 1u))
                : (5 + static_cast<io::i32>((tree_seed >> 17u) % 3u));
            if (trunk_h < 4) trunk_h = 4;

            // Validate local slope under the whole trunk footprint.
            for (io::i32 tz = 0; tz < trunk_w; ++tz) {
                for (io::i32 tx = 0; tx < trunk_w; ++tx) {
                    const io::i32 s = surface_height_at(gx + tx, gz + tz);
                    if (abs_i32(s - surface) > 2) return false;
                }
            }

            if (surface + trunk_h + 6 >= max_world_y)
                return false;

            for (io::i32 y = 1; y <= trunk_h; ++y) {
                const io::i32 wy = surface + y;
                for (io::i32 tz = 0; tz < trunk_w; ++tz) {
                    for (io::i32 tx = 0; tx < trunk_w; ++tx) {
                        if (!tree_append_block(out_blocks, out_cap, out_count,
                                               gx + tx, wy, gz + tz, voxel::BlockId::Log))
                            return false;
                    }
                }
            }

            const io::i32 crown_cx = gx + ((trunk_w == 2) ? 1 : 0);
            const io::i32 crown_cz = gz + ((trunk_w == 2) ? 1 : 0);
            const io::i32 crown_y = surface + trunk_h;

            // Root flare for large trees keeps the base visually heavier but still cheap.
            if (large) {
                static constexpr io::i32 ROOT_DIR[4][2]{
                    { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
                };
                for (io::i32 r = 0; r < 4; ++r) {
                    if (((tree_seed >> (24u + static_cast<io::u32>(r))) & 1u) == 0u) continue;
                    const io::i32 dx = ROOT_DIR[r][0];
                    const io::i32 dz = ROOT_DIR[r][1];
                    const io::i32 len = 1 + static_cast<io::i32>((tree_seed >> (2u + static_cast<io::u32>(r))) & 1u);
                    for (io::i32 s = 1; s <= len; ++s) {
                        const io::i32 rx = crown_cx + dx * s;
                        const io::i32 rz = crown_cz + dz * s;
                        const io::i32 ry = surface + 1 - ((s > 1) ? 1 : 0);
                        if (!tree_append_block(out_blocks, out_cap, out_count, rx, ry, rz, voxel::BlockId::Log))
                            return false;
                    }
                }
            }

            if (large) {
                // Alpha-like large canopy: branch-heavy silhouette with compact center crown.
                static constexpr io::i32 radii[7]{ 1, 2, 2, 3, 2, 2, 1 };
                for (io::i32 dy = -2; dy <= 4; ++dy) {
                    const io::i32 r = radii[dy + 2];
                    tree_emit_leaf_disc(out_blocks, out_cap, out_count,
                                        crown_cx, crown_y + dy, crown_cz, r,
                                        tree_seed ^ static_cast<io::u32>((dy + 5) * 3571));
                }
                tree_emit_leaf_disc(out_blocks, out_cap, out_count, crown_cx, crown_y - 3, crown_cz, 2, tree_seed ^ 0xA17Du);
            } else {
                static constexpr io::i32 radii[5]{ 1, 2, 2, 1, 1 };
                for (io::i32 dy = -2; dy <= 2; ++dy) {
                    const io::i32 r = radii[dy + 2];
                    tree_emit_leaf_disc(out_blocks, out_cap, out_count,
                                        crown_cx, crown_y + dy, crown_cz, r,
                                        tree_seed ^ static_cast<io::u32>((dy + 3) * 2953));
                }
            }

            static constexpr io::i32 dir_x[8]{ 1, 1, 0, -1, -1, -1, 0, 1 };
            static constexpr io::i32 dir_z[8]{ 0, 1, 1, 1, 0, -1, -1, -1 };
            const io::i32 branch_count = large ? (4 + static_cast<io::i32>((tree_seed >> 20u) & 3u))
                                               : (1 + static_cast<io::i32>((tree_seed >> 20u) & 1u));
            for (io::i32 b = 0; b < branch_count; ++b) {
                const io::u32 dir_idx = (tree_seed >> (7u + static_cast<io::u32>(b) * 3u)) & 7u;
                const io::i32 dx = dir_x[dir_idx];
                const io::i32 dz = dir_z[dir_idx];
                const io::i32 len = large
                    ? (3 + static_cast<io::i32>((tree_seed >> (11u + static_cast<io::u32>(b) * 2u)) & 3u))
                    : (2 + static_cast<io::i32>((tree_seed >> (11u + static_cast<io::u32>(b) * 2u)) & 1u));
                io::i32 bx = gx + ((trunk_w == 2 && dx >= 0) ? 1 : 0);
                io::i32 bz = gz + ((trunk_w == 2 && dz >= 0) ? 1 : 0);
                io::i32 by = crown_y - 1 - (b & 1);

                for (io::i32 s = 0; s < len; ++s) {
                    bx += dx;
                    bz += dz;
                    if (large && s > 0 && ((s + b) & 1) == 0)
                        ++by;
                    if (!tree_append_block(out_blocks, out_cap, out_count, bx, by, bz, voxel::BlockId::Log))
                        return false;

                    if (large && s > 0 && (s & 1) == 0 && ((tree_seed >> (18u + static_cast<io::u32>((b + s) & 7))) & 1u) != 0u) {
                        // Small side twigs create more alpha-like branch silhouettes.
                        const io::i32 pdir = (static_cast<io::i32>(dir_idx) + ((s & 2) ? 2 : -2) + 8) & 7;
                        const io::i32 tx = bx + dir_x[pdir];
                        const io::i32 tz = bz + dir_z[pdir];
                        (void)tree_append_block(out_blocks, out_cap, out_count, tx, by, tz, voxel::BlockId::Log);
                        if ((tree_seed & 1u) != 0u)
                            tree_emit_leaf_disc(out_blocks, out_cap, out_count, tx, by + 1, tz, 1, tree_seed ^ static_cast<io::u32>(b * 1777 + s * 991));
                    }
                }
                const io::i32 tip_radius = large
                    ? ((((tree_seed >> (4u + static_cast<io::u32>(b))) & 3u) == 0u) ? 2 : 1)
                    : 1;
                tree_emit_leaf_blob(out_blocks, out_cap, out_count, bx, by, bz, tip_radius,
                                    tree_seed ^ (static_cast<io::u32>(b + 1) * 0x9E37u));
                if (large && tip_radius == 2 && ((tree_seed >> (26u + static_cast<io::u32>(b & 3))) & 1u) != 0u) {
                    const io::i32 drop = 1 + static_cast<io::i32>((tree_seed >> (static_cast<io::u32>(b) + 1u)) & 1u);
                    for (io::i32 d = 1; d <= drop; ++d)
                        tree_emit_leaf_disc(out_blocks, out_cap, out_count, bx, by - d, bz, 1, tree_seed ^ static_cast<io::u32>(0xB001u + b * 13 + d));
                }
            }

            return out_count > 10u && out_count < out_cap;
        }

        inline void place_tree_if_inside_chunk(voxel::ChunkData& out,
                                               io::i32 gx, io::i32 gz,
                                               io::u32 tree_seed) const noexcept {
            const io::i64 chunk_x0 = static_cast<io::i64>(out.coord.x) * static_cast<io::i64>(voxel::CHUNK_W);
            const io::i64 chunk_z0 = static_cast<io::i64>(out.coord.z) * static_cast<io::i64>(voxel::CHUNK_D);
            const io::i64 chunk_x1 = chunk_x0 + static_cast<io::i64>(voxel::CHUNK_W);
            const io::i64 chunk_z1 = chunk_z0 + static_cast<io::i64>(voxel::CHUNK_D);

            const io::i64 min_x = static_cast<io::i64>(gx) - TREE_MAX_RADIUS_XZ;
            const io::i64 max_x = static_cast<io::i64>(gx) + TREE_MAX_RADIUS_XZ + 1;
            const io::i64 min_z = static_cast<io::i64>(gz) - TREE_MAX_RADIUS_XZ;
            const io::i64 max_z = static_cast<io::i64>(gz) + TREE_MAX_RADIUS_XZ + 1;
            if (max_x < chunk_x0 || min_x >= chunk_x1) return;
            if (max_z < chunk_z0 || min_z >= chunk_z1) return;

            TreeBlock blocks[TREE_LAYOUT_MAX_BLOCKS]{};
            io::u32 block_count = 0u;
            bool large = false;
            if (!build_tree_layout(gx, gz, tree_seed, blocks, TREE_LAYOUT_MAX_BLOCKS, block_count, large))
                return;

            for (io::u32 i = 0u; i < block_count; ++i) {
                const TreeBlock& b = blocks[i];
                io::u32 lx = 0, ly = 0, lz = 0;
                if (!world_to_local_if_inside_chunk(out, b.x, b.y, b.z, lx, ly, lz)) continue;
                if (lx >= voxel::CHUNK_W || ly >= voxel::CHUNK_H || lz >= voxel::CHUNK_D) continue;
                const voxel::BlockId cur = out.get(lx, ly, lz);
                if (b.id == voxel::BlockId::Log) {
                    out.set(lx, ly, lz, voxel::BlockId::Log);
                } else if (cur == voxel::BlockId::Air) {
                    out.set(lx, ly, lz, voxel::BlockId::Leaves);
                }
            }
        }

        inline void generate_chunk(voxel::ChunkData& out) const noexcept {
            out.clear();
            const io::i32 chunk_x0 = out.coord.x * static_cast<io::i32>(voxel::CHUNK_W);
            const io::i32 chunk_z0 = out.coord.z * static_cast<io::i32>(voxel::CHUNK_D);
            for (io::u32 z = 0; z < voxel::CHUNK_D; ++z) {
                for (io::u32 x = 0; x < voxel::CHUNK_W; ++x) {
                    const io::i32 gx = chunk_x0 + static_cast<io::i32>(x);
                    const io::i32 gz = chunk_z0 + static_cast<io::i32>(z);
                    const io::i32 surface = surface_height_at(gx, gz);
                    const bool is_beach_column = surface <= sea_level + 1;

                    for (io::u32 y = 0; y < voxel::CHUNK_H; ++y) {
                        const io::i32 gy = out.coord.y * static_cast<io::i32>(voxel::CHUNK_H) + static_cast<io::i32>(y);

                        voxel::BlockId id = voxel::BlockId::Air;
                        if (gy < min_world_y) {
                            id = voxel::BlockId::Stone;
                            out.set(x, y, z, id);
                            continue;
                        }

                        if (gy > surface) {
                            if (gy <= sea_level)
                                id = voxel::BlockId::Water;
                            out.set(x, y, z, id);
                            continue;
                        }

                        const io::i32 depth = surface - gy;
                        if (gy == surface) {
                            if (is_beach_column) id = voxel::BlockId::Sand;
                            else if (gy >= 88) id = voxel::BlockId::Snow;
                            else id = voxel::BlockId::Grass;
                        } else if (depth <= 4) {
                            id = is_beach_column ? voxel::BlockId::Sand : voxel::BlockId::Dirt;
                        } else {
                            id = voxel::BlockId::Stone;
                        }

                        out.set(x, y, z, id);
                    }
                }
            }

            // Stamp tree parts from this and neighbor XZ chunks so trees stay continuous
            // across chunk borders and vertical chunk splits.
            for (io::i32 anchor_cz = out.coord.z - 1; anchor_cz <= out.coord.z + 1; ++anchor_cz) {
                for (io::i32 anchor_cx = out.coord.x - 1; anchor_cx <= out.coord.x + 1; ++anchor_cx) {
                    for (io::u32 attempt = 0; attempt < 4u; ++attempt) {
                        io::i32 gx = 0, gz = 0;
                        io::u32 seed = 0u;
                        if (!tree_anchor_candidate(anchor_cx, anchor_cz, attempt, gx, gz, seed))
                            continue;
                        place_tree_if_inside_chunk(out, gx, gz, seed);
                    }
                }
            }
        }
    };
} // namespace worldgen
} // namespace ge
