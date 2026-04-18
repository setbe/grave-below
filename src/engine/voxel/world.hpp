#pragma once

#include "chunk.hpp"

namespace ge {
namespace voxel {
    IO_NODISCARD static inline io::i32 floor_div_i32(io::i32 value, io::i32 divisor) noexcept {
        if (divisor == 0) return 0;
        io::i32 q = value / divisor;
        const io::i32 r = value % divisor;
        if (r != 0 && ((r < 0) != (divisor < 0))) --q;
        return q;
    }

    IO_NODISCARD static inline io::u32 floor_mod_i32(io::i32 value, io::i32 divisor) noexcept {
        if (divisor == 0) return 0u;
        io::i32 r = value % divisor;
        if (r < 0) r += (divisor < 0 ? -divisor : divisor);
        return static_cast<io::u32>(r);
    }

    static inline void split_world_axis(io::i32 world_value, io::i32& chunk_axis, io::u32& local_axis) noexcept {
        chunk_axis = floor_div_i32(world_value, static_cast<io::i32>(CHUNK_SIZE));
        local_axis = floor_mod_i32(world_value, static_cast<io::i32>(CHUNK_SIZE));
    }

    static inline void split_world_coord(io::i32 wx, io::i32 wy, io::i32 wz,
                                         ChunkCoord& out_chunk, io::u32& lx, io::u32& ly, io::u32& lz) noexcept {
        split_world_axis(wx, out_chunk.x, lx);
        split_world_axis(wy, out_chunk.y, ly);
        split_world_axis(wz, out_chunk.z, lz);
    }

    static inline void chunk_origin_world(const ChunkCoord& c, io::i32& ox, io::i32& oy, io::i32& oz) noexcept {
        ox = c.x * static_cast<io::i32>(CHUNK_SIZE);
        oy = c.y * static_cast<io::i32>(CHUNK_SIZE);
        oz = c.z * static_cast<io::i32>(CHUNK_SIZE);
    }

    struct World {
        struct ChunkLookupEntry {
            ChunkCoord coord{};
            io::usize index = io::npos;
            io::u8 used = 0u;
        };

        io::vector<ChunkData> chunks{};
        io::usize max_chunks{};
        mutable io::vector<ChunkLookupEntry> chunk_lookup{};
        mutable io::u32 chunk_lookup_mask = 0u;
        mutable io::usize chunk_lookup_size_snapshot = 0u;
        mutable bool chunk_lookup_dirty = true;

        IO_NODISCARD static inline io::u32 hash_chunk_coord(const ChunkCoord& coord) noexcept {
            io::u32 x = static_cast<io::u32>(coord.x);
            io::u32 y = static_cast<io::u32>(coord.y);
            io::u32 z = static_cast<io::u32>(coord.z);
            io::u32 h = 2166136261u;
            h ^= x; h *= 16777619u;
            h ^= y; h *= 16777619u;
            h ^= z; h *= 16777619u;
            h ^= h >> 16u;
            h *= 0x7FEB352Du;
            h ^= h >> 15u;
            h *= 0x846CA68Bu;
            h ^= h >> 16u;
            return h;
        }

        IO_NODISCARD static inline io::u32 next_pow2_u32(io::u32 v) noexcept {
            if (v <= 1u) return 1u;
            --v;
            v |= v >> 1u;
            v |= v >> 2u;
            v |= v >> 4u;
            v |= v >> 8u;
            v |= v >> 16u;
            return v + 1u;
        }

        IO_NODISCARD inline bool init(io::usize max_chunk_count) noexcept {
            clear();
            max_chunks = max_chunk_count;
            if (max_chunk_count == 0) return true;
            return chunks.reserve(max_chunk_count);
        }

        inline void invalidate_lookup() const noexcept {
            chunk_lookup_dirty = true;
        }

        IO_NODISCARD inline bool rebuild_lookup() const noexcept {
            chunk_lookup.clear();
            chunk_lookup_mask = 0u;
            chunk_lookup_size_snapshot = chunks.size();
            if (chunks.empty()) {
                chunk_lookup_dirty = false;
                return true;
            }

            io::u32 cap = static_cast<io::u32>(chunks.size());
            if (cap > 0x3FFFFFFFu) cap = 0x3FFFFFFFu;
            cap *= 2u;
            if (cap < 8u) cap = 8u;
            cap = next_pow2_u32(cap);

            if (!chunk_lookup.resize(cap))
                return false;
            for (io::u32 i = 0u; i < cap; ++i) {
                chunk_lookup[i].used = 0u;
                chunk_lookup[i].index = io::npos;
            }
            chunk_lookup_mask = cap - 1u;

            for (io::usize i = 0; i < chunks.size(); ++i) {
                io::u32 pos = hash_chunk_coord(chunks[i].coord) & chunk_lookup_mask;
                bool placed = false;
                for (io::u32 probe = 0u; probe < cap; ++probe) {
                    ChunkLookupEntry& e = chunk_lookup[pos];
                    if (e.used == 0u || coord_eq(e.coord, chunks[i].coord)) {
                        e.coord = chunks[i].coord;
                        e.index = i;
                        e.used = 1u;
                        placed = true;
                        break;
                    }
                    pos = (pos + 1u) & chunk_lookup_mask;
                }
                if (!placed)
                    return false;
            }

            chunk_lookup_dirty = false;
            return true;
        }

        IO_NODISCARD inline bool ensure_lookup_ready() const noexcept {
            if (chunk_lookup_dirty || chunk_lookup_size_snapshot != chunks.size())
                return rebuild_lookup();
            return true;
        }

        IO_NODISCARD inline io::usize lookup_chunk_index(const ChunkCoord& coord) const noexcept {
            if (chunk_lookup.empty() || chunk_lookup_mask == 0u)
                return io::npos;
            const io::u32 cap = static_cast<io::u32>(chunk_lookup.size());
            io::u32 pos = hash_chunk_coord(coord) & chunk_lookup_mask;
            for (io::u32 probe = 0u; probe < cap; ++probe) {
                const ChunkLookupEntry& e = chunk_lookup[pos];
                if (e.used == 0u) return io::npos;
                if (coord_eq(e.coord, coord)) return e.index;
                pos = (pos + 1u) & chunk_lookup_mask;
            }
            return io::npos;
        }

        inline void clear() noexcept {
            chunks.clear();
            chunk_lookup.clear();
            chunk_lookup_mask = 0u;
            chunk_lookup_size_snapshot = 0u;
            chunk_lookup_dirty = true;
        }

        IO_NODISCARD inline io::usize size() const noexcept {
            return chunks.size();
        }

        IO_NODISCARD inline ChunkData* find_chunk(const ChunkCoord& coord) noexcept {
            if (ensure_lookup_ready()) {
                const io::usize idx = lookup_chunk_index(coord);
                if (idx != io::npos && idx < chunks.size() && coord_eq(chunks[idx].coord, coord))
                    return &chunks[idx];
            }
            for (io::usize i = 0; i < chunks.size(); ++i)
                if (coord_eq(chunks[i].coord, coord))
                    return &chunks[i];
            return nullptr;
        }

        IO_NODISCARD inline const ChunkData* find_chunk(const ChunkCoord& coord) const noexcept {
            if (ensure_lookup_ready()) {
                const io::usize idx = lookup_chunk_index(coord);
                if (idx != io::npos && idx < chunks.size() && coord_eq(chunks[idx].coord, coord))
                    return &chunks[idx];
            }
            for (io::usize i = 0; i < chunks.size(); ++i)
                if (coord_eq(chunks[i].coord, coord))
                    return &chunks[i];
            return nullptr;
        }

        IO_NODISCARD inline ChunkData* ensure_chunk(const ChunkCoord& coord) noexcept {
            ChunkData* existing = find_chunk(coord);
            if (existing) return existing;

            if (max_chunks != 0 && chunks.size() >= max_chunks)
                return nullptr;

            const io::usize idx = chunks.size();
            if (!chunks.resize(idx + 1))
                return nullptr;

            ChunkData& chunk = chunks[idx];
            chunk.coord = coord;
            chunk.clear();
            chunk.generated = true;
            invalidate_lookup();
            return &chunk;
        }

        inline bool remove_chunk(const ChunkCoord& coord) noexcept {
            for (io::usize i = 0; i < chunks.size(); ++i) {
                if (!coord_eq(chunks[i].coord, coord))
                    continue;
                const io::usize last = chunks.size() - 1;
                if (i != last)
                    chunks[i] = io::move(chunks[last]);
                chunks.pop_back();
                invalidate_lookup();
                return true;
            }
            return false;
        }

        IO_NODISCARD inline BlockState get_world_state(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            ChunkCoord cc{};
            io::u32 lx = 0;
            io::u32 ly = 0;
            io::u32 lz = 0;
            split_world_coord(wx, wy, wz, cc, lx, ly, lz);
            const ChunkData* chunk = find_chunk(cc);
            if (!chunk) return { block_index(BlockId::Air), 0u };
            return chunk->get_state_at(lx, ly, lz);
        }

        IO_NODISCARD inline BlockId get_world_block(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            const BlockState state = get_world_state(wx, wy, wz);
            if (state.id < BLOCK_COUNT) return static_cast<BlockId>(state.id);
            return BlockId::Air;
        }

        IO_NODISCARD inline io::u16 get_world_state_bits(io::i32 wx, io::i32 wy, io::i32 wz) const noexcept {
            return get_world_state(wx, wy, wz).state;
        }

        inline bool set_world_state(io::i32 wx, io::i32 wy, io::i32 wz,
                                    BlockId id, io::u16 state = 0u, bool create_missing_chunk = true) noexcept {
            ChunkCoord cc{};
            io::u32 lx = 0;
            io::u32 ly = 0;
            io::u32 lz = 0;
            split_world_coord(wx, wy, wz, cc, lx, ly, lz);

            ChunkData* chunk = create_missing_chunk ? ensure_chunk(cc) : find_chunk(cc);
            if (!chunk) return false;

            chunk->set(lx, ly, lz, id, state);
            mark_edge_neighbors_dirty(*chunk, lx, ly, lz);
            return true;
        }

        inline bool set_world_block(io::i32 wx, io::i32 wy, io::i32 wz,
                                    BlockId id, bool create_missing_chunk = true) noexcept {
            return set_world_state(wx, wy, wz, id, 0u, create_missing_chunk);
        }

    private:
        inline void mark_neighbor_dirty(const ChunkCoord& coord) noexcept {
            ChunkData* n = find_chunk(coord);
            if (!n) return;
            n->touch();
            n->dirty_neighbors = true;
        }

        inline void mark_edge_neighbors_dirty(const ChunkData& src, io::u32 lx, io::u32 ly, io::u32 lz) noexcept {
            bool touched = false;

            if (lx == 0) {
                touched = true;
                mark_neighbor_dirty(ChunkCoord{ src.coord.x - 1, src.coord.y, src.coord.z });
            }
            if (lx + 1 == CHUNK_SIZE) {
                touched = true;
                mark_neighbor_dirty(ChunkCoord{ src.coord.x + 1, src.coord.y, src.coord.z });
            }
            if (ly == 0) {
                touched = true;
                mark_neighbor_dirty(ChunkCoord{ src.coord.x, src.coord.y - 1, src.coord.z });
            }
            if (ly + 1 == CHUNK_SIZE) {
                touched = true;
                mark_neighbor_dirty(ChunkCoord{ src.coord.x, src.coord.y + 1, src.coord.z });
            }
            if (lz == 0) {
                touched = true;
                mark_neighbor_dirty(ChunkCoord{ src.coord.x, src.coord.y, src.coord.z - 1 });
            }
            if (lz + 1 == CHUNK_SIZE) {
                touched = true;
                mark_neighbor_dirty(ChunkCoord{ src.coord.x, src.coord.y, src.coord.z + 1 });
            }

            if (touched) {
                ChunkData* self = find_chunk(src.coord);
                if (self) self->dirty_neighbors = true;
            }
        }
    };
} // namespace voxel
} // namespace ge
