#pragma once

#include "fluid.hpp"
#include "world.hpp"

namespace ge {
namespace voxel {
    enum class Face : io::u8 {
        PosX = 0,
        NegX,
        PosY,
        NegY,
        PosZ,
        NegZ
    };

    struct MeshVertex {
        float px{};
        float py{};
        float pz{};
        float nx{};
        float ny{};
        float nz{};
        float u{};
        float v{};
        float atlas_u0{};
        float atlas_v0{};
        float atlas_u1{};
        float atlas_v1{};
        io::u16 block_id{};
        io::u8 face{};
        io::u8 ao{ 255u };
    };

    struct MeshBuffers {
        MeshVertex* vertices{};
        io::u32 vertex_capacity{};
        io::u32 vertex_count{};
        io::u32* indices{};
        io::u32 index_capacity{};
        io::u32 index_count{};

        inline void reset_counts() noexcept {
            vertex_count = 0;
            index_count = 0;
        }
    };

    struct MeshBuildOptions {
        PerceptionLevel perception = PerceptionLevel::Normal;
        bool include_transparent = false;
    };

    struct MeshBuildStats {
        io::u32 blocks_seen{};
        io::u32 blocks_emitted{};
        io::u32 faces_emitted{};
        io::u32 skipped_custom{};
    };

    IO_NODISCARD static inline io::u32 worst_case_faces_per_chunk() noexcept {
        return CHUNK_VOLUME * 6u;
    }

    IO_NODISCARD static inline io::u32 worst_case_vertices_per_chunk() noexcept {
        return worst_case_faces_per_chunk() * 4u;
    }

    IO_NODISCARD static inline io::u32 worst_case_indices_per_chunk() noexcept {
        return worst_case_faces_per_chunk() * 6u;
    }

    IO_NODISCARD static inline BlockId block_id_from_u16(io::u16 raw) noexcept {
        if (raw < BLOCK_COUNT) return static_cast<BlockId>(raw);
        return BlockId::Air;
    }

    IO_NODISCARD static inline bool blocks_face_occluded(BlockId self, BlockId neighbor) noexcept {
        if (neighbor == BlockId::Air) return false;
        const bool self_liquid = is_liquid(self);
        const bool neighbor_liquid = is_liquid(neighbor);
        const bool self_transparent = is_transparent(self);
        const bool neighbor_transparent = is_transparent(neighbor);
        const bool neighbor_full_occluder = is_occluder(neighbor)
            && !neighbor_transparent
            && !neighbor_liquid
            && block_def(neighbor).collision == CollisionKind::FullCube;

        // Liquids: hide internal faces between the same liquid profile
        // (Water/WaterDark, Blood/BloodDark, Slime/SlimeDark).
        if (self_liquid) {
            if (neighbor_full_occluder) return true;
            return neighbor_liquid
                && fluid_kind_from_block_id(self) == fluid_kind_from_block_id(neighbor);
        }

        // Transparent non-liquid blocks (future glass-like): hide internal seams only.
        if (self_transparent && !self_liquid) {
            if (neighbor_full_occluder) return true;
            return !neighbor_liquid && self == neighbor;
        }

        // Opaque blocks must keep boundary face against transparent/liquid neighbors.
        if (neighbor_transparent || neighbor_liquid)
            return false;

        if (!is_occluder(neighbor))
            return false;

        return block_def(self).collision == CollisionKind::FullCube
            && block_def(neighbor).collision == CollisionKind::FullCube;
    }

    IO_NODISCARD static inline bool cull_on_missing_neighbor_chunk(BlockId self) noexcept {
        (void)self;
        // Missing-neighbor culling creates hard square holes while streaming.
        // Prefer visual continuity: render boundary faces until neighbor arrives.
        return false;
    }

    static inline void face_normal(Face face, float& nx, float& ny, float& nz) noexcept {
        nx = 0.f; ny = 0.f; nz = 0.f;
        switch (face) {
        case Face::PosX: nx = 1.f; break;
        case Face::NegX: nx = -1.f; break;
        case Face::PosY: ny = 1.f; break;
        case Face::NegY: ny = -1.f; break;
        case Face::PosZ: nz = 1.f; break;
        case Face::NegZ: nz = -1.f; break;
        }
    }

    static inline void face_offset(Face face, io::i32& dx, io::i32& dy, io::i32& dz) noexcept {
        dx = 0; dy = 0; dz = 0;
        switch (face) {
        case Face::PosX: dx = 1; break;
        case Face::NegX: dx = -1; break;
        case Face::PosY: dy = 1; break;
        case Face::NegY: dy = -1; break;
        case Face::PosZ: dz = 1; break;
        case Face::NegZ: dz = -1; break;
        }
    }

    IO_NODISCARD static inline bool ao_is_occluder(BlockId id) noexcept {
        if (id == BlockId::Air) return false;
        if (is_liquid(id) || is_transparent(id)) return false;
        if (!is_occluder(id)) return false;
        return block_def(id).collision == CollisionKind::FullCube;
    }

    IO_NODISCARD static inline io::u8 ao_level_from_neighbors(bool side1, bool side2, bool corner) noexcept {
        if (side1 && side2) return 0u;
        io::u8 occ = static_cast<io::u8>((side1 ? 1u : 0u) + (side2 ? 1u : 0u) + (corner ? 1u : 0u));
        if (occ > 3u) occ = 3u;
        return static_cast<io::u8>(3u - occ);
    }

    IO_NODISCARD static inline io::u8 ao_level_to_byte(io::u8 level) noexcept {
        static constexpr io::u8 LUT[4]{ 72u, 126u, 184u, 255u };
        if (level > 3u) level = 3u;
        return LUT[level];
    }

    IO_NODISCARD static inline io::u8 pack_ao_levels(io::u8 a0, io::u8 a1, io::u8 a2, io::u8 a3) noexcept {
        return static_cast<io::u8>((a0 & 0x3u) | ((a1 & 0x3u) << 2u) | ((a2 & 0x3u) << 4u) | ((a3 & 0x3u) << 6u));
    }

    IO_NODISCARD static inline io::u8 unpack_ao_level(io::u8 pack, io::u32 corner_index) noexcept {
        return static_cast<io::u8>((pack >> (corner_index * 2u)) & 0x3u);
    }

    static inline void face_vertices(Face face, float x, float y, float z,
                                     float (&p)[4][3]) noexcept {
        const float x0 = x;
        const float y0 = y;
        const float z0 = z;
        const float x1 = x + 1.f;
        const float y1 = y + 1.f;
        const float z1 = z + 1.f;

        switch (face) {
        case Face::PosX:
            p[0][0] = x1; p[0][1] = y0; p[0][2] = z1;
            p[1][0] = x1; p[1][1] = y0; p[1][2] = z0;
            p[2][0] = x1; p[2][1] = y1; p[2][2] = z0;
            p[3][0] = x1; p[3][1] = y1; p[3][2] = z1;
            break;
        case Face::NegX:
            p[0][0] = x0; p[0][1] = y0; p[0][2] = z0;
            p[1][0] = x0; p[1][1] = y0; p[1][2] = z1;
            p[2][0] = x0; p[2][1] = y1; p[2][2] = z1;
            p[3][0] = x0; p[3][1] = y1; p[3][2] = z0;
            break;
        case Face::PosY:
            p[0][0] = x0; p[0][1] = y1; p[0][2] = z1;
            p[1][0] = x1; p[1][1] = y1; p[1][2] = z1;
            p[2][0] = x1; p[2][1] = y1; p[2][2] = z0;
            p[3][0] = x0; p[3][1] = y1; p[3][2] = z0;
            break;
        case Face::NegY:
            p[0][0] = x0; p[0][1] = y0; p[0][2] = z0;
            p[1][0] = x1; p[1][1] = y0; p[1][2] = z0;
            p[2][0] = x1; p[2][1] = y0; p[2][2] = z1;
            p[3][0] = x0; p[3][1] = y0; p[3][2] = z1;
            break;
        case Face::PosZ:
            p[0][0] = x0; p[0][1] = y0; p[0][2] = z1;
            p[1][0] = x1; p[1][1] = y0; p[1][2] = z1;
            p[2][0] = x1; p[2][1] = y1; p[2][2] = z1;
            p[3][0] = x0; p[3][1] = y1; p[3][2] = z1;
            break;
        case Face::NegZ:
            p[0][0] = x1; p[0][1] = y0; p[0][2] = z0;
            p[1][0] = x0; p[1][1] = y0; p[1][2] = z0;
            p[2][0] = x0; p[2][1] = y1; p[2][2] = z0;
            p[3][0] = x1; p[3][1] = y1; p[3][2] = z0;
            break;
        }
    }

    IO_NODISCARD static inline bool push_face_rect_uv(MeshBuffers& out, BlockId visual_id, Face face,
                                                      float x, float y, float z,
                                                      float span_u, float span_v,
                                                      float uv_u0, float uv_v0,
                                                      float uv_u1, float uv_v1,
                                                      io::u8 ao_pack = 0xFFu) noexcept {
        if (!out.vertices || !out.indices) return false;
        if (out.vertex_count + 4u > out.vertex_capacity) return false;
        if (out.index_count + 6u > out.index_capacity) return false;

        float nx = 0.f, ny = 0.f, nz = 0.f;
        face_normal(face, nx, ny, nz);

        float pos[4][3]{};
        const float x0 = x;
        const float y0 = y;
        const float z0 = z;
        const float x1 = x + 1.f;
        const float y1 = y + 1.f;
        const float z1 = z + 1.f;

        switch (face) {
        case Face::PosX: {
            const float yy0 = y0;
            const float yy1 = y0 + span_u;
            const float zz0 = z0;
            const float zz1 = z0 + span_v;
            pos[0][0] = x1; pos[0][1] = yy0; pos[0][2] = zz1;
            pos[1][0] = x1; pos[1][1] = yy0; pos[1][2] = zz0;
            pos[2][0] = x1; pos[2][1] = yy1; pos[2][2] = zz0;
            pos[3][0] = x1; pos[3][1] = yy1; pos[3][2] = zz1;
            break;
        }
        case Face::NegX: {
            const float yy0 = y0;
            const float yy1 = y0 + span_u;
            const float zz0 = z0;
            const float zz1 = z0 + span_v;
            pos[0][0] = x0; pos[0][1] = yy0; pos[0][2] = zz0;
            pos[1][0] = x0; pos[1][1] = yy0; pos[1][2] = zz1;
            pos[2][0] = x0; pos[2][1] = yy1; pos[2][2] = zz1;
            pos[3][0] = x0; pos[3][1] = yy1; pos[3][2] = zz0;
            break;
        }
        case Face::PosY: {
            const float xx0 = x0;
            const float xx1 = x0 + span_u;
            const float zz0 = z0;
            const float zz1 = z0 + span_v;
            pos[0][0] = xx0; pos[0][1] = y1; pos[0][2] = zz1;
            pos[1][0] = xx1; pos[1][1] = y1; pos[1][2] = zz1;
            pos[2][0] = xx1; pos[2][1] = y1; pos[2][2] = zz0;
            pos[3][0] = xx0; pos[3][1] = y1; pos[3][2] = zz0;
            break;
        }
        case Face::NegY: {
            const float xx0 = x0;
            const float xx1 = x0 + span_u;
            const float zz0 = z0;
            const float zz1 = z0 + span_v;
            pos[0][0] = xx0; pos[0][1] = y0; pos[0][2] = zz0;
            pos[1][0] = xx1; pos[1][1] = y0; pos[1][2] = zz0;
            pos[2][0] = xx1; pos[2][1] = y0; pos[2][2] = zz1;
            pos[3][0] = xx0; pos[3][1] = y0; pos[3][2] = zz1;
            break;
        }
        case Face::PosZ: {
            const float xx0 = x0;
            const float xx1 = x0 + span_u;
            const float yy0 = y0;
            const float yy1 = y0 + span_v;
            pos[0][0] = xx0; pos[0][1] = yy0; pos[0][2] = z1;
            pos[1][0] = xx1; pos[1][1] = yy0; pos[1][2] = z1;
            pos[2][0] = xx1; pos[2][1] = yy1; pos[2][2] = z1;
            pos[3][0] = xx0; pos[3][1] = yy1; pos[3][2] = z1;
            break;
        }
        case Face::NegZ: {
            const float xx0 = x0;
            const float xx1 = x0 + span_u;
            const float yy0 = y0;
            const float yy1 = y0 + span_v;
            pos[0][0] = xx1; pos[0][1] = yy0; pos[0][2] = z0;
            pos[1][0] = xx0; pos[1][1] = yy0; pos[1][2] = z0;
            pos[2][0] = xx0; pos[2][1] = yy1; pos[2][2] = z0;
            pos[3][0] = xx1; pos[3][1] = yy1; pos[3][2] = z0;
            break;
        }
        }

        const float uv[4][2]{
            { uv_u0, uv_v0 },
            { uv_u1, uv_v0 },
            { uv_u1, uv_v1 },
            { uv_u0, uv_v1 }
        };

        const io::u16 bid = block_index(visual_id);
        const io::u8 face_i = static_cast<io::u8>(face);
        const io::u32 base = out.vertex_count;

        for (io::u32 i = 0; i < 4u; ++i) {
            MeshVertex& v = out.vertices[out.vertex_count++];
            v.px = pos[i][0];
            v.py = pos[i][1];
            v.pz = pos[i][2];
            v.nx = nx;
            v.ny = ny;
            v.nz = nz;
            v.u = uv[i][0];
            v.v = uv[i][1];
            v.atlas_u0 = 0.f;
            v.atlas_v0 = 0.f;
            v.atlas_u1 = 1.f;
            v.atlas_v1 = 1.f;
            v.block_id = bid;
            v.face = face_i;
            v.ao = ao_level_to_byte(unpack_ao_level(ao_pack, i));
        }

        out.indices[out.index_count++] = base + 0u;
        out.indices[out.index_count++] = base + 1u;
        out.indices[out.index_count++] = base + 2u;
        out.indices[out.index_count++] = base + 2u;
        out.indices[out.index_count++] = base + 3u;
        out.indices[out.index_count++] = base + 0u;
        return true;
    }

    IO_NODISCARD static inline bool push_face_rect(MeshBuffers& out, BlockId visual_id, Face face,
                                                   float x, float y, float z,
                                                   float span_u, float span_v,
                                                   io::u8 ao_pack = 0xFFu) noexcept {
        return push_face_rect_uv(out, visual_id, face, x, y, z,
                                 span_u, span_v,
                                 0.f, 0.f, span_u, span_v, ao_pack);
    }

    IO_NODISCARD static inline bool push_face(MeshBuffers& out, BlockId visual_id, Face face,
                                              float x, float y, float z) noexcept {
        return push_face_rect(out, visual_id, face, x, y, z, 1.f, 1.f, 0xFFu);
    }

    struct GreedyRect {
        io::u32 u{};
        io::u32 v{};
        io::u32 w{};
        io::u32 h{};
    };

    IO_NODISCARD static inline io::u32 trailing_zeros_u32(io::u32 value) noexcept {
        if (value == 0u) return 32u;
        io::u32 n = 0u;
        while ((value & 1u) == 0u) {
            value >>= 1u;
            ++n;
        }
        return n;
    }

    IO_NODISCARD static inline io::u32 trailing_ones_u32(io::u32 value) noexcept {
        io::u32 n = 0u;
        while ((value & 1u) != 0u) {
            value >>= 1u;
            ++n;
        }
        return n;
    }

    static inline void greedy_mesh_binary_plane(const io::u32 (&rows)[CHUNK_SIZE],
                                                GreedyRect* out_rects,
                                                io::u32 out_cap,
                                                io::u32& out_count) noexcept {
        out_count = 0u;
        io::u32 work[CHUNK_SIZE]{};
        for (io::u32 i = 0; i < CHUNK_SIZE; ++i)
            work[i] = rows[i];

        for (io::u32 row = 0u; row < CHUNK_SIZE; ++row) {
            io::u32 bit = 0u;
            while (bit < CHUNK_SIZE) {
                bit += trailing_zeros_u32(work[row] >> bit);
                if (bit >= CHUNK_SIZE) continue;

                const io::u32 h = trailing_ones_u32(work[row] >> bit);
                const io::u32 h_mask = (h >= 32u) ? 0xFFFFFFFFu : ((1u << h) - 1u);
                const io::u32 mask = h_mask << bit;

                io::u32 w = 1u;
                while (row + w < CHUNK_SIZE) {
                    const io::u32 next_row_h = (work[row + w] >> bit) & h_mask;
                    if (next_row_h != h_mask) break;
                    work[row + w] &= ~mask;
                    ++w;
                }

                if (out_count < out_cap)
                    out_rects[out_count++] = GreedyRect{ row, bit, w, h };
                bit += h;
            }
        }
    }

    static inline void face_cell_to_local(Face face, io::u32 slice, io::u32 u, io::u32 v,
                                          io::u32& x, io::u32& y, io::u32& z) noexcept {
        switch (face) {
        case Face::PosX:
        case Face::NegX:
            x = slice; y = u; z = v;
            return;
        case Face::PosY:
        case Face::NegY:
            x = u; y = slice; z = v;
            return;
        case Face::PosZ:
        case Face::NegZ:
            x = u; y = v; z = slice;
            return;
        }
        x = 0u; y = 0u; z = 0u;
    }

    static inline void face_rect_to_world(Face face,
                                          io::i32 ox, io::i32 oy, io::i32 oz,
                                          io::u32 slice, const GreedyRect& rect,
                                          io::i32& wx, io::i32& wy, io::i32& wz,
                                          float& span_u, float& span_v) noexcept {
        switch (face) {
        case Face::PosX:
        case Face::NegX:
            wx = ox + static_cast<io::i32>(slice);
            wy = oy + static_cast<io::i32>(rect.u);
            wz = oz + static_cast<io::i32>(rect.v);
            span_u = static_cast<float>(rect.w);
            span_v = static_cast<float>(rect.h);
            return;
        case Face::PosY:
        case Face::NegY:
            wx = ox + static_cast<io::i32>(rect.u);
            wy = oy + static_cast<io::i32>(slice);
            wz = oz + static_cast<io::i32>(rect.v);
            span_u = static_cast<float>(rect.w);
            span_v = static_cast<float>(rect.h);
            return;
        case Face::PosZ:
        case Face::NegZ:
            wx = ox + static_cast<io::i32>(rect.u);
            wy = oy + static_cast<io::i32>(rect.v);
            wz = oz + static_cast<io::i32>(slice);
            span_u = static_cast<float>(rect.w);
            span_v = static_cast<float>(rect.h);
            return;
        }
        wx = ox; wy = oy; wz = oz;
        span_u = 1.f;
        span_v = 1.f;
    }

    struct NeighborSample {
        BlockId id{ BlockId::Air };
        io::u16 state{};
        bool crossed_chunk_boundary{};
        bool neighbor_chunk_loaded{ true };
    };

    struct NeighborChunkCache {
        const ChunkData* cells[3][3][3]{};
    };

    static inline void build_neighbor_chunk_cache(const World* world,
                                                  const ChunkData& chunk,
                                                  NeighborChunkCache& cache) noexcept {
        for (io::u32 x = 0u; x < 3u; ++x)
            for (io::u32 y = 0u; y < 3u; ++y)
                for (io::u32 z = 0u; z < 3u; ++z)
                    cache.cells[x][y][z] = nullptr;
        cache.cells[1][1][1] = &chunk;
        if (!world) return;

        for (io::i32 ox = -1; ox <= 1; ++ox) {
            for (io::i32 oy = -1; oy <= 1; ++oy) {
                for (io::i32 oz = -1; oz <= 1; ++oz) {
                    if (ox == 0 && oy == 0 && oz == 0)
                        continue;
                    const ChunkCoord cc{
                        chunk.coord.x + ox,
                        chunk.coord.y + oy,
                        chunk.coord.z + oz
                    };
                    cache.cells[ox + 1][oy + 1][oz + 1] = world->find_chunk(cc);
                }
            }
        }
    }

    IO_NODISCARD static inline NeighborSample sample_neighbor(const World* world, const ChunkData& chunk,
                                                              io::u32 x, io::u32 y, io::u32 z,
                                                              io::i32 dx, io::i32 dy, io::i32 dz,
                                                              const NeighborChunkCache* cache = nullptr) noexcept {
        const io::i32 nx = static_cast<io::i32>(x) + dx;
        const io::i32 ny = static_cast<io::i32>(y) + dy;
        const io::i32 nz = static_cast<io::i32>(z) + dz;

        if (nx >= 0 && ny >= 0 && nz >= 0 &&
            nx < static_cast<io::i32>(CHUNK_W) &&
            ny < static_cast<io::i32>(CHUNK_H) &&
            nz < static_cast<io::i32>(CHUNK_D)) {
            NeighborSample s{};
            const io::u32 li = chunk_index(static_cast<io::u32>(nx),
                                           static_cast<io::u32>(ny),
                                           static_cast<io::u32>(nz));
            const io::u16 id = chunk.blocks[li].id;
            s.id = (id < BLOCK_COUNT) ? static_cast<BlockId>(id) : BlockId::Air;
            s.state = chunk.blocks[li].state;
            s.crossed_chunk_boundary = false;
            s.neighbor_chunk_loaded = true;
            return s;
        }

        NeighborSample s{};
        s.id = BlockId::Air;
        s.crossed_chunk_boundary = true;
        s.neighbor_chunk_loaded = false;
        if (!world && !cache) return s;

        io::i32 local_x = nx;
        io::i32 local_y = ny;
        io::i32 local_z = nz;
        io::i32 off_x = 0;
        io::i32 off_y = 0;
        io::i32 off_z = 0;

        if (local_x < 0) {
            local_x += static_cast<io::i32>(CHUNK_W);
            off_x = -1;
        } else if (local_x >= static_cast<io::i32>(CHUNK_W)) {
            local_x -= static_cast<io::i32>(CHUNK_W);
            off_x = 1;
        }

        if (local_y < 0) {
            local_y += static_cast<io::i32>(CHUNK_H);
            off_y = -1;
        } else if (local_y >= static_cast<io::i32>(CHUNK_H)) {
            local_y -= static_cast<io::i32>(CHUNK_H);
            off_y = 1;
        }

        if (local_z < 0) {
            local_z += static_cast<io::i32>(CHUNK_D);
            off_z = -1;
        } else if (local_z >= static_cast<io::i32>(CHUNK_D)) {
            local_z -= static_cast<io::i32>(CHUNK_D);
            off_z = 1;
        }

        const ChunkData* neighbor_chunk = nullptr;
        if (cache) {
            neighbor_chunk = cache->cells[off_x + 1][off_y + 1][off_z + 1];
        } else if (world) {
            const ChunkCoord cc{
                chunk.coord.x + off_x,
                chunk.coord.y + off_y,
                chunk.coord.z + off_z
            };
            neighbor_chunk = world->find_chunk(cc);
        }
        if (!neighbor_chunk) return s;

        const io::u32 li = chunk_index(static_cast<io::u32>(local_x),
                                       static_cast<io::u32>(local_y),
                                       static_cast<io::u32>(local_z));
        const io::u16 id = neighbor_chunk->blocks[li].id;
        s.id = (id < BLOCK_COUNT) ? static_cast<BlockId>(id) : BlockId::Air;
        s.state = neighbor_chunk->blocks[li].state;
        s.neighbor_chunk_loaded = true;
        return s;
    }

    IO_NODISCARD static inline io::u8 compute_face_ao_pack(const World* world, const ChunkData& chunk,
                                                            Face face, io::u32 x, io::u32 y, io::u32 z,
                                                            const NeighborChunkCache* cache = nullptr) noexcept {
        io::i32 nx = 0, ny = 0, nz = 0;
        io::i32 ux = 0, uy = 0, uz = 0;
        io::i32 vx = 0, vy = 0, vz = 0;
        io::i8 su[4]{};
        io::i8 sv[4]{};

        switch (face) {
        case Face::PosX:
            nx = 1; ny = 0; nz = 0;
            ux = 0; uy = 1; uz = 0;
            vx = 0; vy = 0; vz = 1;
            su[0] = -1; sv[0] = +1;
            su[1] = -1; sv[1] = -1;
            su[2] = +1; sv[2] = -1;
            su[3] = +1; sv[3] = +1;
            break;
        case Face::NegX:
            nx = -1; ny = 0; nz = 0;
            ux = 0; uy = 1; uz = 0;
            vx = 0; vy = 0; vz = 1;
            su[0] = -1; sv[0] = -1;
            su[1] = -1; sv[1] = +1;
            su[2] = +1; sv[2] = +1;
            su[3] = +1; sv[3] = -1;
            break;
        case Face::PosY:
            nx = 0; ny = 1; nz = 0;
            ux = 1; uy = 0; uz = 0;
            vx = 0; vy = 0; vz = 1;
            su[0] = -1; sv[0] = +1;
            su[1] = +1; sv[1] = +1;
            su[2] = +1; sv[2] = -1;
            su[3] = -1; sv[3] = -1;
            break;
        case Face::NegY:
            nx = 0; ny = -1; nz = 0;
            ux = 1; uy = 0; uz = 0;
            vx = 0; vy = 0; vz = 1;
            su[0] = -1; sv[0] = -1;
            su[1] = +1; sv[1] = -1;
            su[2] = +1; sv[2] = +1;
            su[3] = -1; sv[3] = +1;
            break;
        case Face::PosZ:
            nx = 0; ny = 0; nz = 1;
            ux = 1; uy = 0; uz = 0;
            vx = 0; vy = 1; vz = 0;
            su[0] = -1; sv[0] = -1;
            su[1] = +1; sv[1] = -1;
            su[2] = +1; sv[2] = +1;
            su[3] = -1; sv[3] = +1;
            break;
        case Face::NegZ:
            nx = 0; ny = 0; nz = -1;
            ux = 1; uy = 0; uz = 0;
            vx = 0; vy = 1; vz = 0;
            su[0] = +1; sv[0] = -1;
            su[1] = -1; sv[1] = -1;
            su[2] = -1; sv[2] = +1;
            su[3] = +1; sv[3] = +1;
            break;
        }

        io::u8 ao[4]{ 3u, 3u, 3u, 3u };
        for (io::u32 i = 0; i < 4u; ++i) {
            const io::i32 s1x = nx + static_cast<io::i32>(su[i]) * ux;
            const io::i32 s1y = ny + static_cast<io::i32>(su[i]) * uy;
            const io::i32 s1z = nz + static_cast<io::i32>(su[i]) * uz;
            const io::i32 s2x = nx + static_cast<io::i32>(sv[i]) * vx;
            const io::i32 s2y = ny + static_cast<io::i32>(sv[i]) * vy;
            const io::i32 s2z = nz + static_cast<io::i32>(sv[i]) * vz;
            const io::i32 cx = nx + static_cast<io::i32>(su[i]) * ux + static_cast<io::i32>(sv[i]) * vx;
            const io::i32 cy = ny + static_cast<io::i32>(su[i]) * uy + static_cast<io::i32>(sv[i]) * vy;
            const io::i32 cz = nz + static_cast<io::i32>(su[i]) * uz + static_cast<io::i32>(sv[i]) * vz;

            const bool side1 = ao_is_occluder(sample_neighbor(world, chunk, x, y, z, s1x, s1y, s1z, cache).id);
            const bool side2 = ao_is_occluder(sample_neighbor(world, chunk, x, y, z, s2x, s2y, s2z, cache).id);
            const bool corner = ao_is_occluder(sample_neighbor(world, chunk, x, y, z, cx, cy, cz, cache).id);
            ao[i] = ao_level_from_neighbors(side1, side2, corner);
        }
        return pack_ao_levels(ao[0], ao[1], ao[2], ao[3]);
    }

    IO_NODISCARD static inline FluidStack liquid_stack(BlockId id, io::u16 state) noexcept {
        return fluid_stack_from_block(id, state);
    }

    IO_NODISCARD static inline float liquid_height(BlockId id, io::u16 state) noexcept {
        const FluidStack s = liquid_stack(id, state);
        const io::u8 level = fluid_total_level(s);
        if (level == 0u) return 0.f;
        return static_cast<float>(level) * (1.f / 8.f);
    }

    IO_NODISCARD static inline bool emit_liquid_cell_faces(const World* world,
                                                            const ChunkData& chunk,
                                                            MeshBuffers& out,
                                                            BlockId visual_id,
                                                            io::u32 x, io::u32 y, io::u32 z,
                                                            io::u16 state,
                                                            const NeighborChunkCache* cache = nullptr) noexcept {
        const FluidStack stack = liquid_stack(visual_id, state);
        const io::u8 total_level = fluid_total_level(stack);
        if (total_level == 0u) return true;
        const io::u8 top_level = stack.top_level;
        const io::u8 bottom_level = static_cast<io::u8>(total_level - top_level);

        const float h = static_cast<float>(total_level) * (1.f / 8.f);
        const float h_top = static_cast<float>(top_level) * (1.f / 8.f);
        const float h_bottom = static_cast<float>(bottom_level) * (1.f / 8.f);

        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fz = static_cast<float>(z);
        const float liquid_top_y = fy + h;
        const float liquid_mid_y = fy + h_bottom;
        static constexpr float EPS = 0.0001f;

        const FluidKind top_kind = (stack.top_kind != FluidKind::None && top_level > 0u) ? stack.top_kind : stack.bottom_kind;
        const FluidKind bottom_kind = (stack.bottom_kind != FluidKind::None && bottom_level > 0u) ? stack.bottom_kind : FluidKind::None;
        const BlockId top_id = (top_kind != FluidKind::None) ? fluid_dark_block_id(top_kind) : visual_id;
        const BlockId bottom_id = (bottom_kind != FluidKind::None) ? fluid_dark_block_id(bottom_kind) : visual_id;

        const NeighborSample up = sample_neighbor(world, chunk, x, y, z, 0, 1, 0, cache);
        const float up_h = is_liquid(up.id) ? liquid_height(up.id, up.state) : 0.f;
        bool emit_top_face = false;
        if (!is_liquid(up.id)) {
            if (total_level < FLUID_LEVEL_MAX) {
                // Layered liquids keep their top cap under non-liquid neighbors.
                emit_top_face = true;
            } else {
                emit_top_face = ((up.id == BlockId::Air || (is_transparent(up.id) && !is_liquid(up.id))) && up_h <= EPS);
            }
        } else {
            // Density rule for fluid-fluid contact:
            // only the lower-density side of a shared boundary is culled.
            const FluidStack up_stack = liquid_stack(up.id, up.state);
            const FluidKind self_upper_kind = (top_kind != FluidKind::None) ? top_kind : bottom_kind;
            FluidKind up_lower_kind = FluidKind::None;
            if (up_stack.bottom_kind != FluidKind::None && up_stack.bottom_level > 0u)
                up_lower_kind = up_stack.bottom_kind;
            else if (up_stack.top_kind != FluidKind::None && up_stack.top_level > 0u)
                up_lower_kind = up_stack.top_kind;

            if (self_upper_kind != FluidKind::None && up_lower_kind != FluidKind::None) {
                if (self_upper_kind == up_lower_kind)
                    emit_top_face = false;
                else
                    emit_top_face = fluid_density(self_upper_kind) > fluid_density(up_lower_kind);
            } else {
                emit_top_face = true;
            }
        }
        if (emit_top_face) {
            if (!push_face_rect(out, top_id, Face::PosY, fx, liquid_top_y - 1.f, fz, 1.f, 1.f, 0xFFu))
                return false;
        }

        // Mixed liquid stacks keep an internal interface so blood/slime inside water
        // is still visible as a distinct layer through transparency.
        if (h_top > EPS && h_bottom > EPS && top_kind != bottom_kind) {
            if (!push_face_rect(out, bottom_id, Face::PosY, fx, liquid_mid_y - 1.f, fz, 1.f, 1.f, 0xFFu))
                return false;
            if (!push_face_rect(out, top_id, Face::NegY, fx, liquid_mid_y, fz, 1.f, 1.f, 0xFFu))
                return false;
        }

        const NeighborSample down = sample_neighbor(world, chunk, x, y, z, 0, -1, 0, cache);
        const float down_h = is_liquid(down.id) ? liquid_height(down.id, down.state) : 0.f;
        bool emit_bottom_face = false;
        if (!is_liquid(down.id)) {
            emit_bottom_face = (down.id == BlockId::Air && down_h <= EPS);
        } else {
            const FluidStack down_stack = liquid_stack(down.id, down.state);
            const FluidKind self_lower_kind = (bottom_kind != FluidKind::None) ? bottom_kind : top_kind;
            FluidKind down_upper_kind = FluidKind::None;
            if (down_stack.top_kind != FluidKind::None && down_stack.top_level > 0u)
                down_upper_kind = down_stack.top_kind;
            else if (down_stack.bottom_kind != FluidKind::None && down_stack.bottom_level > 0u)
                down_upper_kind = down_stack.bottom_kind;

            if (self_lower_kind != FluidKind::None && down_upper_kind != FluidKind::None) {
                if (self_lower_kind == down_upper_kind)
                    emit_bottom_face = false;
                else
                    emit_bottom_face = fluid_density(self_lower_kind) > fluid_density(down_upper_kind);
            } else {
                emit_bottom_face = true;
            }
        }
        if (emit_bottom_face) {
            if (!push_face_rect(out, bottom_id, Face::NegY, fx, fy, fz, 1.f, 1.f, 0xFFu))
                return false;
        }

        const struct {
            Face face;
            io::i32 dx;
            io::i32 dy;
            io::i32 dz;
        } sides[4]{
            { Face::PosX, 1, 0, 0 },
            { Face::NegX, -1, 0, 0 },
            { Face::PosZ, 0, 0, 1 },
            { Face::NegZ, 0, 0, -1 }
        };

        for (io::u32 i = 0u; i < 4u; ++i) {
            const NeighborSample n = sample_neighbor(world, chunk, x, y, z, sides[i].dx, sides[i].dy, sides[i].dz, cache);
            const bool n_is_liquid = is_liquid(n.id);
            FluidStack n_stack{};
            io::u8 n_total = 0u;
            FluidKind n_top_kind = FluidKind::None;
            FluidKind n_bottom_kind = FluidKind::None;
            float n_bottom_h = 0.f;
            float n_top_h = 0.f;
            if (n_is_liquid) {
                n_stack = liquid_stack(n.id, n.state);
                n_total = fluid_total_level(n_stack);
                n_top_kind = (n_stack.top_kind != FluidKind::None && n_stack.top_level > 0u) ? n_stack.top_kind : n_stack.bottom_kind;
                n_bottom_kind = (n_stack.bottom_kind != FluidKind::None && n_stack.bottom_level > 0u) ? n_stack.bottom_kind : FluidKind::None;
                n_bottom_h = static_cast<float>(n_stack.bottom_level) * (1.f / 8.f);
                n_top_h = static_cast<float>(n_stack.top_level) * (1.f / 8.f);
            }
            if (n_is_liquid) {
                const bool self_mixed =
                    stack.top_kind != FluidKind::None
                    && stack.top_level > 0u
                    && stack.bottom_kind != FluidKind::None
                    && stack.bottom_level > 0u
                    && stack.top_kind != stack.bottom_kind;
                const bool n_mixed =
                    n_stack.top_kind != FluidKind::None
                    && n_stack.top_level > 0u
                    && n_stack.bottom_kind != FluidKind::None
                    && n_stack.bottom_level > 0u
                    && n_stack.top_kind != n_stack.bottom_kind;
                const bool same_full_single =
                    !self_mixed
                    && !n_mixed
                    && total_level == FLUID_LEVEL_MAX
                    && n_total == FLUID_LEVEL_MAX
                    && top_kind != FluidKind::None
                    && top_kind == n_top_kind;
                if (same_full_single)
                    continue;
            }

            auto emit_segment = [&](FluidKind seg_kind, BlockId seg_id, float y0, float y1) noexcept -> bool {
                if (y1 - y0 <= EPS) return true;
                float o0 = 0.f;
                float o1 = 0.f;
                bool has_overlap = false;
                if (n_is_liquid && seg_kind != FluidKind::None) {
                    const io::u8 seg_density = fluid_density(seg_kind);
                    auto extend_overlap = [&](float a, float b) noexcept {
                        if (b <= y0 + EPS || a >= y1 - EPS) return;
                        if (a < y0) a = y0;
                        if (b > y1) b = y1;
                        if (b - a <= EPS) return;
                        if (!has_overlap) {
                            o0 = a;
                            o1 = b;
                            has_overlap = true;
                            return;
                        }
                        if (a < o0) o0 = a;
                        if (b > o1) o1 = b;
                    };

                    auto maybe_overlap = [&](FluidKind n_kind, float a, float b) noexcept {
                        if (n_kind == FluidKind::None) return;
                        const io::u8 n_density = fluid_density(n_kind);
                        // Cull only lower-density side (plus same-kind internal seam).
                        if (n_kind == seg_kind || seg_density < n_density)
                            extend_overlap(a, b);
                    };

                    if (n_bottom_h > EPS)
                        maybe_overlap(n_bottom_kind, fy, fy + n_bottom_h);
                    if (n_top_h > EPS)
                        maybe_overlap(n_top_kind, fy + n_bottom_h, fy + n_bottom_h + n_top_h);
                }

                auto emit_part = [&](float a, float b) noexcept -> bool {
                    const float part_h = b - a;
                    if (part_h <= EPS) return true;
                    const float uv0 = a - y0;
                    const float uv1 = b - y0;
                    if (sides[i].face == Face::PosX || sides[i].face == Face::NegX)
                        return push_face_rect_uv(out, seg_id, sides[i].face, fx, a, fz,
                                                 part_h, 1.f,
                                                 uv0, 0.f, uv1, 1.f, 0xFFu);
                    return push_face_rect_uv(out, seg_id, sides[i].face, fx, a, fz,
                                             1.f, part_h,
                                             0.f, uv0, 1.f, uv1, 0xFFu);
                };

                if (!has_overlap)
                    return emit_part(y0, y1);

                if (!emit_part(y0, o0)) return false;
                if (!emit_part(o1, y1)) return false;
                return true;
            };

            if (h_top > EPS) {
                if (!emit_segment(top_kind, top_id, liquid_mid_y, liquid_top_y))
                    return false;
            }

            if (h_bottom > EPS) {
                if (!emit_segment(bottom_kind, bottom_id, fy, liquid_mid_y))
                    return false;
            }
        }
        return true;
    }

    IO_NODISCARD static inline bool build_chunk_mesh(const World* world, const ChunkData& chunk,
                                                     MeshBuffers& out, const MeshBuildOptions& opt = {},
                                                     MeshBuildStats* stats = nullptr,
                                                     io::u32 (*rows_by_block_scratch)[CHUNK_SIZE] = nullptr,
                                                     GreedyRect* rects_scratch = nullptr) noexcept {
        if (stats) *stats = {};
        out.reset_counts();

        if (stats) {
            for (io::u32 y = 0; y < CHUNK_H; ++y) {
                for (io::u32 z = 0; z < CHUNK_D; ++z) {
                    const BlockState* row = chunk.row_ptr(y, z);
                    for (io::u32 x = 0; x < CHUNK_W; ++x) {
                        const BlockId canonical = block_id_from_u16(row[x].id);
                        if (canonical == BlockId::Air) continue;
                        ++stats->blocks_seen;

                        const BlockId visual = remap_perception(canonical, opt.perception);
                        const BlockDef& def = block_def(visual);
                        if (def.render == RenderKind::Empty) continue;
                        if (def.render == RenderKind::CustomModel) {
                            ++stats->skipped_custom;
                            continue;
                        }
                        if (!opt.include_transparent && is_transparent(visual)) continue;
                        ++stats->blocks_emitted;
                    }
                }
            }
        }

        (void)rows_by_block_scratch;
        (void)rects_scratch;
        io::u32 plane_keys[CHUNK_SIZE][CHUNK_SIZE]{};
        io::u8 consumed[CHUNK_SIZE][CHUNK_SIZE]{};
        NeighborChunkCache neighbor_cache{};
        build_neighbor_chunk_cache(world, chunk, neighbor_cache);

        // Liquids are rendered as layered cells (1..8) for smoother water steps.
        if (opt.include_transparent) {
            for (io::u32 y = 0u; y < CHUNK_H; ++y) {
                for (io::u32 z = 0u; z < CHUNK_D; ++z) {
                    const BlockState* row = chunk.row_ptr(y, z);
                    for (io::u32 x = 0u; x < CHUNK_W; ++x) {
                        const io::u16 raw = row[x].id;
                        const BlockId canonical = (raw < BLOCK_COUNT) ? static_cast<BlockId>(raw) : BlockId::Air;
                        if (!is_liquid(canonical))
                            continue;
                        const BlockId visual = remap_perception(canonical, opt.perception);
                        if (!is_liquid(visual))
                            continue;
                        const io::u32 faces_before = out.index_count / 6u;
                        if (!emit_liquid_cell_faces(world, chunk, out, visual, x, y, z, row[x].state, &neighbor_cache))
                            return false;
                        if (stats) {
                            const io::u32 faces_after = out.index_count / 6u;
                            if (faces_after > faces_before)
                                stats->faces_emitted += (faces_after - faces_before);
                        }
                    }
                }
            }
        }

        for (io::u32 fi = 0; fi < 6u; ++fi) {
            const Face face = static_cast<Face>(fi);
            io::i32 dx = 0, dy = 0, dz = 0;
            face_offset(face, dx, dy, dz);

            for (io::u32 slice = 0; slice < CHUNK_SIZE; ++slice) {
                for (io::u32 u = 0u; u < CHUNK_SIZE; ++u)
                    for (io::u32 v = 0u; v < CHUNK_SIZE; ++v) {
                        plane_keys[u][v] = 0u;
                        consumed[u][v] = 0u;
                    }

                for (io::u32 u = 0u; u < CHUNK_SIZE; ++u) {
                    for (io::u32 v = 0u; v < CHUNK_SIZE; ++v) {
                        io::u32 x = 0u, y = 0u, z = 0u;
                        face_cell_to_local(face, slice, u, v, x, y, z);

                        const io::u32 bi = chunk_index(x, y, z);
                        const io::u16 cid = chunk.blocks[bi].id;
                        const BlockId canonical = (cid < BLOCK_COUNT) ? static_cast<BlockId>(cid) : BlockId::Air;
                        if (canonical == BlockId::Air) continue;

                        const BlockId visual = remap_perception(canonical, opt.perception);
                        const BlockDef& def = block_def(visual);
                        if (def.render == RenderKind::Empty || def.render == RenderKind::CustomModel)
                            continue;
                        if (is_liquid(visual))
                            continue;
                        if (!opt.include_transparent && is_transparent(visual))
                            continue;

                        const NeighborSample neighbor = sample_neighbor(world, chunk, x, y, z, dx, dy, dz, &neighbor_cache);

                        // At render-distance boundary we intentionally close chunk shells.
                        if (neighbor.crossed_chunk_boundary && !neighbor.neighbor_chunk_loaded &&
                            cull_on_missing_neighbor_chunk(canonical))
                            continue;
                        if (blocks_face_occluded(canonical, neighbor.id))
                            continue;

                        const io::u16 bid = block_index(visual);
                        if (bid >= BLOCK_COUNT) continue;
                        const io::u8 ao_pack = compute_face_ao_pack(world, chunk, face, x, y, z, &neighbor_cache);
                        plane_keys[u][v] = static_cast<io::u32>(bid + 1u) | (static_cast<io::u32>(ao_pack) << 16u);
                    }
                }

                for (io::u32 u = 0u; u < CHUNK_SIZE; ++u) {
                    for (io::u32 v = 0u; v < CHUNK_SIZE; ++v) {
                        const io::u32 key = plane_keys[u][v];
                        if (key == 0u || consumed[u][v] != 0u) continue;

                        io::u32 h = 1u;
                        while (v + h < CHUNK_SIZE && plane_keys[u][v + h] == key && consumed[u][v + h] == 0u)
                            ++h;

                        io::u32 w = 1u;
                        while (u + w < CHUNK_SIZE) {
                            bool row_match = true;
                            for (io::u32 vv = 0u; vv < h; ++vv) {
                                if (plane_keys[u + w][v + vv] == key && consumed[u + w][v + vv] == 0u)
                                    continue;
                                row_match = false;
                                break;
                            }
                            if (!row_match) break;
                            ++w;
                        }

                        for (io::u32 du = 0u; du < w; ++du)
                            for (io::u32 dv = 0u; dv < h; ++dv)
                                consumed[u + du][v + dv] = 1u;

                        const io::u16 bid = static_cast<io::u16>((key & 0xFFFFu) - 1u);
                        const io::u8 ao_pack = static_cast<io::u8>((key >> 16u) & 0xFFu);
                        if (bid >= BLOCK_COUNT) continue;

                        const GreedyRect rect{ u, v, w, h };
                        io::i32 wx = 0, wy = 0, wz = 0;
                        float span_u = 1.f, span_v = 1.f;
                        // Keep chunk vertices in local-chunk space [0..CHUNK_SIZE].
                        // World placement is applied later via per-chunk model translation.
                        face_rect_to_world(face, 0, 0, 0, slice, rect, wx, wy, wz, span_u, span_v);
                        if (!push_face_rect(out, static_cast<BlockId>(bid), face,
                                            static_cast<float>(wx),
                                            static_cast<float>(wy),
                                            static_cast<float>(wz),
                                            span_u, span_v, ao_pack))
                            return false;
                        if (stats) ++stats->faces_emitted;
                    }
                }
            }
        }

        return true;
    }
} // namespace voxel
} // namespace ge
