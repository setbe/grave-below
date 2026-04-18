#pragma once

#include "hi/hi/io.hpp"

namespace ge {
    struct DtHistory {
        static constexpr int CAP = 256;

        float samples[CAP]{};
        int index = 0;
        int count = 0;
        float sum = 0.f;

        inline void clear() noexcept {
            for (int i = 0; i < CAP; ++i)
                samples[i] = 0.f;
            index = 0;
            count = 0;
            sum = 0.f;
        }

        inline void push(float dt) noexcept {
            sum -= samples[index];
            samples[index] = dt;
            sum += dt;
            ++index;
            if (index >= CAP) index = 0;
            if (count < CAP) ++count;
        }

        IO_NODISCARD inline float avg_dt() const noexcept {
            if (count <= 0) return 0.f;
            return sum / static_cast<float>(count);
        }

        IO_NODISCARD inline float avg_fps() const noexcept {
            const float dt = avg_dt();
            return dt > 0.000001f ? (1.f / dt) : 0.f;
        }
    };
}

