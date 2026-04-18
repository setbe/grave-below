#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#include "server_stream_budget.hpp"

namespace ge {
namespace server {
namespace chunk {
    io::u32 CountInflight(const io::u8* states,
                          io::u32 count,
                          io::u8 queued_state,
                          io::u8 await_state) noexcept {
        if (!states || count == 0u) return 0u;
        io::u32 n = 0u;
        for (io::u32 i = 0; i < count; ++i) {
            const io::u8 st = states[i];
            if (st == queued_state || st == await_state)
                ++n;
        }
        return n;
    }

    io::u32 ClampPeerBudget(io::u32 hot_chunks, io::u32 stream_count) noexcept {
        io::u32 b = hot_chunks;
        if (b < 1u) b = 1u;
        if (stream_count > 0u && b > stream_count) b = stream_count;
        return b;
    }
}
}
}
