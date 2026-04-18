#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#include "server_stream_policy.hpp"

namespace {
    static inline io::i32 slot_ox(io::u32 slot, io::u32 sx, io::u32 sz, io::u32 distance) noexcept {
        const io::u32 plane = sx * sz;
        const io::u32 iy = slot / plane;
        (void)iy;
        const io::u32 rem = slot % plane;
        const io::u32 ix = rem % sx;
        return static_cast<io::i32>(ix) - static_cast<io::i32>(distance);
    }

    static inline io::i32 slot_oy(io::u32 slot, io::u32 sx, io::u32 sz, io::i32 y_radius) noexcept {
        const io::u32 plane = sx * sz;
        const io::u32 iy = slot / plane;
        return static_cast<io::i32>(iy) - y_radius;
    }

    static inline io::i32 slot_oz(io::u32 slot, io::u32 sx, io::u32 sz, io::u32 distance) noexcept {
        const io::u32 plane = sx * sz;
        (void)plane;
        const io::u32 rem = slot % (sx * sz);
        const io::u32 iz = rem / sx;
        return static_cast<io::i32>(iz) - static_cast<io::i32>(distance);
    }

    static inline io::u32 slot_d2(io::u32 slot,
                                   io::u32 sx,
                                   io::u32 sz,
                                   io::u32 distance,
                                   io::i32 y_radius) noexcept {
        const io::i32 ox = slot_ox(slot, sx, sz, distance);
        const io::i32 oy = slot_oy(slot, sx, sz, y_radius);
        const io::i32 oz = slot_oz(slot, sx, sz, distance);
        return static_cast<io::u32>(ox * ox + oy * oy + oz * oz);
    }
}

namespace ge {
namespace server {
namespace chunk {
    io::i32 ComputeStreamYRadius(io::u32 distance_chunks) noexcept {
        io::i32 y = 3;
        if (distance_chunks >= 4u) y = 4;
        if (distance_chunks >= 12u) y = 5;
        if (distance_chunks >= 20u) y = 6;
        if (y < 3) y = 3;
        if (y > 6) y = 6;
        return y;
    }

    io::u32 StreamPriority(io::i32 ox, io::i32 oy, io::i32 oz) noexcept {
        const io::u32 horiz = static_cast<io::u32>(ox * ox + oz * oz);
        const io::u32 vert = static_cast<io::u32>(oy * oy);
        return horiz * 4u + vert * 25u;
    }

    void BuildSortedStreamOrder(io::u32* order,
                                io::u32 count,
                                io::u32 sx,
                                io::u32 sz,
                                io::u32 distance_chunks,
                                io::i32 y_radius) noexcept {
        if (!order || count == 0u) return;
        for (io::u32 i = 0; i < count; ++i)
            order[i] = i;

        for (io::u32 i = 1; i < count; ++i) {
            const io::u32 key = order[i];
            const io::i32 key_ox = slot_ox(key, sx, sz, distance_chunks);
            const io::i32 key_oy = slot_oy(key, sx, sz, y_radius);
            const io::i32 key_oz = slot_oz(key, sx, sz, distance_chunks);
            const io::u32 key_pri = StreamPriority(key_ox, key_oy, key_oz);
            const io::u32 key_d2 = slot_d2(key, sx, sz, distance_chunks, y_radius);
            io::u32 j = i;
            while (j > 0u) {
                const io::u32 left = order[j - 1u];
                const io::i32 lx = slot_ox(left, sx, sz, distance_chunks);
                const io::i32 ly = slot_oy(left, sx, sz, y_radius);
                const io::i32 lz = slot_oz(left, sx, sz, distance_chunks);
                const io::u32 left_pri = StreamPriority(lx, ly, lz);
                if (left_pri < key_pri) break;
                if (left_pri == key_pri &&
                    slot_d2(left, sx, sz, distance_chunks, y_radius) <= key_d2)
                    break;
                order[j] = order[j - 1u];
                --j;
            }
            order[j] = key;
        }
    }
}
}
}
