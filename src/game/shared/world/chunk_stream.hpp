#pragma once

#include "../net/protocol.hpp"

namespace ge {
namespace net {
    struct ChunkAssembly {
        bool active{};
        io::u32 request_id{};
        voxel::ChunkData chunk{};

        io::u32 expected_total_bytes{};
        io::u16 expected_parts{};
        io::u16 bytes_per_part{};
        io::u8 expected_encoding = CHUNK_WIRE_ENCODING_RAW;

        io::u16 received_parts{};
        io::u8 received_mask[CHUNK_MAX_PARTS]{};
        io::u8 rle_payload[CHUNK_WIRE_BLOCK_BYTES_RAW_STATE]{};

        inline void reset() noexcept {
            active = false;
            request_id = 0;
            expected_total_bytes = 0;
            expected_parts = 0;
            bytes_per_part = 0;
            expected_encoding = CHUNK_WIRE_ENCODING_RAW;
            received_parts = 0;
            for (io::u16 i = 0; i < CHUNK_MAX_PARTS; ++i)
                received_mask[i] = 0;
            // Keep reset lightweight and stack-safe for freestanding Mini builds.
            // Full block buffer clear is only required for explicit implicit-air chunks.
            chunk.coord = {};
            chunk.non_air_count = 0;
            chunk.version = 0;
            chunk.dirty_mesh = false;
            chunk.dirty_neighbors = false;
            chunk.generated = false;
        }

        IO_NODISCARD inline bool begin(const ChunkBegin& begin_msg) noexcept {
            reset();
            const bool is_implicit_air = begin_msg.total_bytes == 0u;
            if (is_implicit_air) {
                if (begin_msg.part_count != 0u) return false;
                if (begin_msg.bytes_per_part != 0u) return false;
                if (begin_msg.encoding != CHUNK_WIRE_ENCODING_RAW) return false;
            } else {
                if (begin_msg.encoding == CHUNK_WIRE_ENCODING_RAW) {
                    if (begin_msg.total_bytes != CHUNK_WIRE_BLOCK_BYTES) return false;
                } else if (begin_msg.encoding == CHUNK_WIRE_ENCODING_NIBBLE) {
                    if (begin_msg.total_bytes != CHUNK_WIRE_BLOCK_BYTES_NIBBLE) return false;
                } else if (begin_msg.encoding == CHUNK_WIRE_ENCODING_RAW_STATE) {
                    if (begin_msg.total_bytes != CHUNK_WIRE_BLOCK_BYTES_RAW_STATE) return false;
                } else if (begin_msg.encoding == CHUNK_WIRE_ENCODING_RLE_STATE) {
                    if (begin_msg.total_bytes == 0u || begin_msg.total_bytes > CHUNK_WIRE_BLOCK_BYTES_RAW_STATE) return false;
                } else {
                    return false;
                }
                if (begin_msg.part_count == 0u || begin_msg.part_count > CHUNK_MAX_PARTS) return false;
                if (begin_msg.bytes_per_part == 0u) return false;
                if (part_count_for(begin_msg.total_bytes, begin_msg.bytes_per_part) != begin_msg.part_count) return false;
            }

            active = true;
            request_id = begin_msg.request_id;
            chunk.coord = begin_msg.coord;
            expected_total_bytes = begin_msg.total_bytes;
            expected_parts = begin_msg.part_count;
            bytes_per_part = begin_msg.bytes_per_part;
            expected_encoding = begin_msg.encoding;
            if (is_implicit_air)
                chunk.clear();
            return true;
        }

        static inline io::u16 decode_wire_id(io::u8 wire_id) noexcept {
            if (wire_id < voxel::BLOCK_COUNT) return static_cast<io::u16>(wire_id);
            return voxel::block_index(voxel::BlockId::Air);
        }

        IO_NODISCARD inline bool add_part(const ChunkPart& part, io::byte_view payload) noexcept {
            if (!active) return false;
            if (part.request_id != request_id) return false;
            if (part.part_index >= expected_parts) return false;
            if (part.part_size != payload.size()) return false;
            if (part.part_size == 0) return false;
            if (received_mask[part.part_index]) return true;

            const io::u32 dst_offset = static_cast<io::u32>(part.part_index) * static_cast<io::u32>(bytes_per_part);
            if (dst_offset + part.part_size > expected_total_bytes) return false;

            if (expected_encoding == CHUNK_WIRE_ENCODING_RAW) {
                for (io::u32 i = 0; i < part.part_size; ++i) {
                    const io::u32 block_idx = dst_offset + i;
                    chunk.blocks[block_idx].id = decode_wire_id(payload[i]);
                    chunk.blocks[block_idx].state = 0u;
                }
            } else if (expected_encoding == CHUNK_WIRE_ENCODING_NIBBLE) {
                for (io::u32 i = 0; i < part.part_size; ++i) {
                    const io::u32 packed_idx = dst_offset + i;
                    const io::u32 block_idx = packed_idx * 2u;
                    const io::u8 packed = payload[i];
                    if (block_idx < voxel::CHUNK_VOLUME) {
                        chunk.blocks[block_idx].id = decode_wire_id(static_cast<io::u8>(packed & 0x0Fu));
                        chunk.blocks[block_idx].state = 0u;
                    }
                    if (block_idx + 1u < voxel::CHUNK_VOLUME) {
                        chunk.blocks[block_idx + 1u].id = decode_wire_id(static_cast<io::u8>((packed >> 4u) & 0x0Fu));
                        chunk.blocks[block_idx + 1u].state = 0u;
                    }
                }
            } else if (expected_encoding == CHUNK_WIRE_ENCODING_RAW_STATE) {
                for (io::u32 i = 0; i + 2u < part.part_size; i += 3u) {
                    const io::u32 byte_idx = dst_offset + i;
                    const io::u32 block_idx = byte_idx / 3u;
                    if (block_idx >= voxel::CHUNK_VOLUME) break;
                    chunk.blocks[block_idx].id = decode_wire_id(payload[i + 0u]);
                    chunk.blocks[block_idx].state =
                        static_cast<io::u16>((static_cast<io::u16>(payload[i + 1u]) << 8u) |
                                             static_cast<io::u16>(payload[i + 2u]));
                }
            } else if (expected_encoding == CHUNK_WIRE_ENCODING_RLE_STATE) {
                for (io::u32 i = 0; i < part.part_size; ++i)
                    rle_payload[dst_offset + i] = payload[i];
            } else {
                return false;
            }

            received_mask[part.part_index] = 1;
            ++received_parts;
            return true;
        }

        IO_NODISCARD inline bool is_complete() const noexcept {
            return active && received_parts == expected_parts;
        }

        IO_NODISCARD inline bool end(const ChunkEnd& end_msg) noexcept {
            if (!active) return false;
            if (end_msg.request_id != request_id) return false;
            if (end_msg.total_bytes != expected_total_bytes) return false;
            if (!is_complete()) return false;

            if (expected_encoding == CHUNK_WIRE_ENCODING_RLE_STATE) {
                io::u32 cursor = 0u;
                io::u32 block_index = 0u;
                while (cursor + 4u < expected_total_bytes) {
                    const io::u16 run = static_cast<io::u16>((static_cast<io::u16>(rle_payload[cursor + 0u]) << 8u) |
                                                              static_cast<io::u16>(rle_payload[cursor + 1u]));
                    const io::u8 bid = rle_payload[cursor + 2u];
                    const io::u16 state = static_cast<io::u16>((static_cast<io::u16>(rle_payload[cursor + 3u]) << 8u) |
                                                                static_cast<io::u16>(rle_payload[cursor + 4u]));
                    cursor += 5u;
                    if (run == 0u) return false;
                    for (io::u16 j = 0u; j < run; ++j) {
                        if (block_index >= voxel::CHUNK_VOLUME) return false;
                        chunk.blocks[block_index].id = decode_wire_id(bid);
                        chunk.blocks[block_index].state = state;
                        ++block_index;
                    }
                }
                if (cursor != expected_total_bytes) return false;
                if (block_index != voxel::CHUNK_VOLUME) return false;
            }

            io::u32 got_hash = voxel::hash_chunk_ids_fnv1a32(chunk);
            if (expected_encoding == CHUNK_WIRE_ENCODING_RAW_STATE || expected_encoding == CHUNK_WIRE_ENCODING_RLE_STATE)
                got_hash = voxel::hash_chunk_ids_and_state_fnv1a32(chunk);
            if (got_hash != end_msg.hash) return false;

            chunk.non_air_count = 0;
            for (io::u32 i = 0; i < voxel::CHUNK_VOLUME; ++i)
                if (chunk.wire_block_id_at(i) != static_cast<io::u8>(voxel::BlockId::Air)) ++chunk.non_air_count;
            return true;
        }
    };
} // namespace net
} // namespace ge
