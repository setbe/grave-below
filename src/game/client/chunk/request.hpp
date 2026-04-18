#pragma once

#include "../../../engine/voxel/chunk.hpp"
#include "hi/hi/hi.hpp"

namespace ge {
namespace client {
namespace chunk {
    IO_NODISCARD static inline io::u32 ComputeRequestsPerFrame(io::u32 render_distance_chunks) noexcept {
        io::u32 n = 8 + render_distance_chunks / 2;
        return (n<8) ? 8 : ((n>48) ? 48 : n);
    } // ComputeRequestsPerFrame

    IO_NODISCARD static inline io::u32 ComputeOutstandingLimit(io::usize virtual_count,
                                                                io::usize max_chunks) noexcept {
        io::u32 lim = 96u;
        if (max_chunks > 0u) {
            io::usize m = max_chunks / 4u;
            m = (m<32) ? 32 : ((m>512) ? 512 : m);
            lim = static_cast<io::u32>(m);
        }
        if (virtual_count > 0u && lim > virtual_count)
            lim = static_cast<io::u32>(virtual_count);
        if (lim < 24u) lim = 24u;
        return lim;
    } // ComputeOutstandingLimit

    IO_NODISCARD static inline bool BuildNearestOffsets(io::vector<ge::voxel::ChunkCoord>& out,
                                                         io::i32 rx,
                                                         io::i32 ry,
                                                         io::i32 rz) noexcept {
        if (rx < 0 || ry < 0 || rz < 0)
            return false;

        const io::i32 max_d2_i = rx * rx + ry * ry + rz * rz;
        if (max_d2_i < 0)
            return false;
        const io::usize max_d2 = static_cast<io::usize>(max_d2_i);

        io::vector<io::u32> bins{};
        if (!bins.resize(max_d2 + 1u))
            return false;
        for (io::usize i = 0; i < bins.size(); ++i)
            bins[i] = 0u;

        io::usize total = 0u;
        for (io::i32 oy = -ry; oy <= ry; ++oy)
            for (io::i32 oz = -rz; oz <= rz; ++oz)
                for (io::i32 ox = -rx; ox <= rx; ++ox) {
                    const io::usize d2 = static_cast<io::usize>(ox * ox + oy * oy + oz * oz);
                    ++bins[d2];
                    ++total;
                }

        io::u32 running = 0u;
        for (io::usize i = 0; i < bins.size(); ++i) {
            const io::u32 c = bins[i];
            bins[i] = running;
            running += c;
        }

        out.clear();
        if (!out.resize(total))
            return false;

        for (io::i32 oy = -ry; oy <= ry; ++oy)
            for (io::i32 oz = -rz; oz <= rz; ++oz)
                for (io::i32 ox = -rx; ox <= rx; ++ox) {
                    const io::usize d2 = static_cast<io::usize>(ox * ox + oy * oy + oz * oz);
                    const io::usize dst = static_cast<io::usize>(bins[d2]++);
                    out[dst] = ge::voxel::ChunkCoord{ ox, oy, oz };
                }

        return true;
    } // BuildNearestOffsets
} // namespace ge
} // namespace client
} // namespace chunk
