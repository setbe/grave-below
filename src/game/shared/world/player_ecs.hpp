#pragma once

#include "hi/hi/hi.hpp"

namespace ge {
namespace ecs {
    struct CPlayerTransform {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct CPlayerVelocity {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct CPlayerHealth {
        io::u16 hp = 100u;
    };

    struct CPlayerFlags {
        bool use_fly = false;
        bool use_noclip = false;
        bool grounded = false;
        bool airborne = false;
    };

    struct CPlayerRuntime {
        float air_peak_foot_y = 0.f;
        io::u32 airborne_ms = 0u;
    };

    template<io::u32 CAPACITY>
    struct PlayerEcs {
        static constexpr io::u32 CAP = CAPACITY;
        io::u8 alive[CAP]{};
        CPlayerTransform transform[CAP]{};
        CPlayerVelocity velocity[CAP]{};
        CPlayerHealth health[CAP]{};
        CPlayerFlags flags[CAP]{};
        CPlayerRuntime runtime[CAP]{};

        inline void Reset() noexcept {
            for (io::u32 i = 0; i < CAP; ++i) {
                alive[i] = 0u;
                transform[i] = {};
                velocity[i] = {};
                health[i] = {};
                flags[i] = {};
                runtime[i] = {};
            }
        }

        inline void Activate(io::u32 i) noexcept {
            if (i >= CAP) return;
            alive[i] = 1u;
            transform[i] = {};
            velocity[i] = {};
            health[i] = {};
            flags[i] = {};
            runtime[i] = {};
        }

        inline void Deactivate(io::u32 i) noexcept {
            if (i >= CAP) return;
            alive[i] = 0u;
            transform[i] = {};
            velocity[i] = {};
            health[i] = {};
            flags[i] = {};
            runtime[i] = {};
        }
    };
}
}

