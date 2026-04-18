#pragma once

#include "block.hpp"

namespace ge {
namespace voxel {
    static constexpr io::u32 CHUNK_SIZE = 32u;
    static constexpr io::u32 CHUNK_W = CHUNK_SIZE;
    static constexpr io::u32 CHUNK_H = CHUNK_SIZE;
    static constexpr io::u32 CHUNK_D = CHUNK_SIZE;
    static constexpr io::u32 CHUNK_STRIDE_Z = CHUNK_SIZE;
    static constexpr io::u32 CHUNK_STRIDE_Y = CHUNK_SIZE * CHUNK_SIZE;
    static constexpr io::u32 CHUNK_VOLUME = CHUNK_W * CHUNK_H * CHUNK_D;

    struct ChunkCoord {
        io::i32 x{};
        io::i32 y{};
        io::i32 z{};
    };

    IO_NODISCARD static inline bool coord_eq(const ChunkCoord& a, const ChunkCoord& b) noexcept {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    IO_NODISCARD static inline io::u32 chunk_index(io::u32 x, io::u32 y, io::u32 z) noexcept {
        return x + z * CHUNK_STRIDE_Z + y * CHUNK_STRIDE_Y;
    }

    struct ChunkData {
        ChunkCoord coord{};
        BlockState blocks[CHUNK_VOLUME]{};
        io::u32 non_air_count{};
        io::u32 version{};
        bool dirty_mesh{};
        bool dirty_neighbors{};
        bool generated{};

        inline void touch() noexcept {
            ++version;
            dirty_mesh = true;
        }

        inline void clear() noexcept {
            for (io::u32 i = 0; i < CHUNK_VOLUME; ++i) {
                blocks[i].id = block_index(BlockId::Air);
                blocks[i].state = 0u;
            }
            non_air_count = 0;
            ++version;
            dirty_mesh = true;
            dirty_neighbors = false;
            generated = false;
        }

        inline void set_linear(io::u32 idx, BlockId id, io::u16 state = 0u) noexcept {
            if (idx >= CHUNK_VOLUME) return;
            const io::u16 next_id = block_index(block_id_or_air(id));
            const io::u16 prev_id = blocks[idx].id;
            const io::u16 prev_state = blocks[idx].state;
            blocks[idx].id = next_id;
            blocks[idx].state = state;
            if (prev_id == block_index(BlockId::Air) && next_id != block_index(BlockId::Air)) ++non_air_count;
            if (prev_id != block_index(BlockId::Air) && next_id == block_index(BlockId::Air)) --non_air_count;
            if (prev_id != next_id || prev_state != state) touch();
        }

        inline void set(io::u32 x, io::u32 y, io::u32 z, BlockId id, io::u16 state = 0u) noexcept {
            set_linear(chunk_index(x, y, z), id, state);
        }

        inline void set_wire_block_id_at(io::u32 idx, io::u8 wire_id) noexcept {
            set_linear(idx, static_cast<BlockId>(wire_id), 0u);
        }

        IO_NODISCARD inline BlockState get_state_at(io::u32 x, io::u32 y, io::u32 z) const noexcept {
            return blocks[chunk_index(x, y, z)];
        }

        IO_NODISCARD inline BlockId get(io::u32 x, io::u32 y, io::u32 z) const noexcept {
            const io::u16 id = blocks[chunk_index(x, y, z)].id;
            if (id < BLOCK_COUNT) return static_cast<BlockId>(id);
            return BlockId::Air;
        }

        IO_NODISCARD inline io::u16 get_state_bits(io::u32 x, io::u32 y, io::u32 z) const noexcept {
            return blocks[chunk_index(x, y, z)].state;
        }

        IO_NODISCARD inline io::u8 wire_block_id_at(io::u32 idx) const noexcept {
            if (idx >= CHUNK_VOLUME) return static_cast<io::u8>(BlockId::Air);
            return static_cast<io::u8>(blocks[idx].id & 0xFFu);
        }

        IO_NODISCARD inline BlockState* row_ptr(io::u32 y, io::u32 z) noexcept {
            return &blocks[chunk_index(0u, y, z)];
        }

        IO_NODISCARD inline const BlockState* row_ptr(io::u32 y, io::u32 z) const noexcept {
            return &blocks[chunk_index(0u, y, z)];
        }
    };

    IO_NODISCARD static inline io::u32 hash_chunk_fnv1a32(const io::u8* bytes, io::u32 size) noexcept {
        io::u32 hash = 2166136261u;
        for (io::u32 i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 16777619u;
        }
        return hash;
    }

    IO_NODISCARD static inline io::u32 hash_chunk_ids_fnv1a32(const ChunkData& chunk) noexcept {
        io::u32 hash = 2166136261u;
        for (io::u32 i = 0; i < CHUNK_VOLUME; ++i) {
            hash ^= chunk.wire_block_id_at(i);
            hash *= 16777619u;
        }
        return hash;
    }

    IO_NODISCARD static inline io::u32 hash_chunk_ids_and_state_fnv1a32(const ChunkData& chunk) noexcept {
        io::u32 hash = 2166136261u;
        for (io::u32 i = 0; i < CHUNK_VOLUME; ++i) {
            const io::u8 bid = chunk.wire_block_id_at(i);
            const io::u16 state = chunk.blocks[i].state;
            hash ^= bid;
            hash *= 16777619u;
            hash ^= static_cast<io::u8>((state >> 8u) & 0xFFu);
            hash *= 16777619u;
            hash ^= static_cast<io::u8>(state & 0xFFu);
            hash *= 16777619u;
        }
        return hash;
    }
} // namespace voxel
} // namespace ge
