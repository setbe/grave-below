#pragma once

#include "hi/hi/hi.hpp"

#include "../../engine/core/config_text.hpp"
#include "../../engine/voxel/chunk.hpp"
#include "../../engine/voxel/chunk_storage.hpp"
#include "../../engine/voxel/fluid.hpp"
#include "../../engine/voxel/world.hpp"
#include "../../engine/worldgen/simplex_worldgen.hpp"
#include "chunk/server_stream_budget.hpp"
#include "chunk/server_stream_policy.hpp"
#include "../shared/net/protocol.hpp"
#include "../shared/world/item.hpp"
#include "../shared/world/actor_ecs.hpp"
#include "../shared/world/player_ecs.hpp"
#include "../shared/world/block_build_profile.hpp"

namespace {
    template<class T>
    static bool read_payload_exact(io::byte_view payload, T& out) noexcept {
        if (payload.size() != sizeof(T)) return false;
        io::u8* dst = reinterpret_cast<io::u8*>(&out);
        for (io::usize i = 0; i < sizeof(T); ++i)
            dst[i] = payload[i];
        return true;
    }

    static inline io::i32 floor_to_i32(float value) noexcept {
        io::i32 out = static_cast<io::i32>(value);
        if (static_cast<float>(out) > value) --out;
        return out;
    }

    static inline float absf(float value) noexcept {
        return value < 0.f ? -value : value;
    }

    static inline bool is_finite_f32(float value) noexcept {
        // NaN check: NaN != NaN; Inf check via finite max bounds.
        if (!(value == value)) return false;
        const float max_finite = 3.402823466e38f;
        return value <= max_finite && value >= -max_finite;
    }

    static inline bool is_reasonable_world_pos(float x, float y, float z) noexcept {
        if (!is_finite_f32(x) || !is_finite_f32(y) || !is_finite_f32(z))
            return false;
        // Safety envelope to avoid pathological world coords in server simulation.
        const float world_limit = 1048576.0f; // 2^20 blocks
        if (absf(x) > world_limit) return false;
        if (absf(y) > world_limit) return false;
        if (absf(z) > world_limit) return false;
        return true;
    }

        static inline io::i32 abs_i32(io::i32 value) noexcept {
            return value < 0 ? -value : value;
        }

    static inline bool is_space_char(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static inline io::char_view trim_view(io::char_view view) noexcept {
        io::usize a = 0;
        io::usize b = view.size();
        while (a < b && is_space_char(view[a])) ++a;
        while (b > a && is_space_char(view[b - 1])) --b;
        return view.slice(a, b - a);
    }

    static inline io::char_view pop_token(io::char_view& io_text) noexcept {
        io_text = trim_view(io_text);
        if (io_text.empty()) return {};
        io::usize n = 0;
        while (n < io_text.size() && !is_space_char(io_text[n])) ++n;
        const io::char_view tok = io_text.slice(0, n);
        io_text = io_text.slice(n, io_text.size() - n);
        return tok;
    }

    static inline char to_lower_ascii(char c) noexcept {
        if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
        return c;
    }

    static inline bool eq_icase(io::char_view a, io::char_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (io::usize i = 0; i < a.size(); ++i)
            if (to_lower_ascii(a[i]) != to_lower_ascii(b[i]))
                return false;
        return true;
    }

    static inline bool parse_i32_token(io::char_view tok, io::i32& out) noexcept {
        tok = trim_view(tok);
        if (tok.empty()) return false;
        io::usize i = 0;
        bool neg = false;
        if (tok[i] == '+' || tok[i] == '-') {
            neg = (tok[i] == '-');
            ++i;
        }
        if (i >= tok.size()) return false;
        io::u32 value = 0u;
        const io::u32 limit = neg ? 2147483648u : 2147483647u;
        for (; i < tok.size(); ++i) {
            const char ch = tok[i];
            if (ch < '0' || ch > '9') return false;
            const io::u32 digit = static_cast<io::u32>(ch - '0');
            if (value > (limit - digit) / 10u) return false;
            value = value * 10u + digit;
        }
        if (neg) {
            if (value == 2147483648u) out = (-2147483647 - 1);
            else out = -static_cast<io::i32>(value);
        } else {
            out = static_cast<io::i32>(value);
        }
        return true;
    }

struct ServerApp {
#include "app/server_app_members.hpp"
#include "app/server_app_regions.hpp"
#include "app/server_app_inventory.hpp"
#include "app/server_app_world_sim_sand.hpp"
#include "app/server_app_fluid_and_gameplay.hpp"
#include "app/server_app_runtime_pipeline.hpp"
#include "app/server_app_tick_and_callbacks.hpp"
    };
}

