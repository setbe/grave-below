#pragma once

#include "config.hpp"

namespace ge {
    enum class ServerTextConfigState : io::u8 {
        LoadedCurrent = 0,
        Created,
        Updated,
        Failed
    };

    struct ServerTextConfig {
        io::i32 job_threads = 0; // <= 0 means "use max_threads"
        io::i32 distance_chunks = 10;
        io::i32 hot_chunks = 1; // simulation radius in chunks (1 => 3x3x3)
        io::i32 day_time_multiply = 20; // base day(1 min) and night(0.75 min) multiplier
        ServerTextConfigState state = ServerTextConfigState::Failed;
    };

    IO_NODISCARD static inline io::char_view server_text_config_state_to_string(ServerTextConfigState state) noexcept {
        switch (state) {
        case ServerTextConfigState::LoadedCurrent: return "loaded-current";
        case ServerTextConfigState::Created:       return "created";
        case ServerTextConfigState::Updated:       return "updated";
        default:                                   return "failed";
        }
    }

    static io::char_view FILENAME_SERVER_CONFIG_TXT{ "config.txt" };

    static inline bool is_ascii_space(char ch) noexcept {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }

    IO_NODISCARD static inline io::char_view trim_ascii(io::char_view value) noexcept {
        io::usize begin = 0;
        io::usize end = value.size();
        while (begin < end && is_ascii_space(value[begin])) ++begin;
        while (end > begin && is_ascii_space(value[end - 1])) --end;
        if (end <= begin) return {};
        return value.slice(begin, end - begin);
    }

    static inline char ascii_lower(char ch) noexcept {
        if (ch >= 'A' && ch <= 'Z') return static_cast<char>(ch - 'A' + 'a');
        return ch;
    }

    IO_NODISCARD static inline bool iequals_ascii(io::char_view a, io::char_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (io::usize i = 0; i < a.size(); ++i)
            if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
        return true;
    }

    IO_NODISCARD static inline bool parse_config_field_value(io::char_view field,
                                                              io::char_view line,
                                                              io::char_view& out_value) noexcept {
        out_value = {};
        const io::char_view t = trim_ascii(line);
        if (t.empty()) return false;
        if (t[0] == '#' || t[0] == ';') return false;
        if (t.size() >= 2 && t[0] == '/' && t[1] == '/') return false;
        if (field.empty()) return false;
        if (t.size() < field.size()) return false;

        for (io::usize i = 0; i < field.size(); ++i)
            if (t[i] != field[i]) return false;

        io::usize pos = field.size();
        while (pos < t.size() && is_ascii_space(t[pos])) ++pos;
        if (pos >= t.size() || t[pos] != '=') return false;
        ++pos;
        out_value = trim_ascii(t.slice(pos, t.size() - pos));
        if (!out_value.empty()) {
            io::usize cut = out_value.size();
            for (io::usize i = 0; i < out_value.size(); ++i) {
                const char ch = out_value[i];
                if (ch == '#' || ch == ';') {
                    cut = i;
                    break;
                }
                if (ch == '/' && (i + 1u) < out_value.size() && out_value[i + 1u] == '/') {
                    cut = i;
                    break;
                }
            }
            out_value = trim_ascii(out_value.slice(0, cut));
        }
        return !out_value.empty();
    }

    IO_NODISCARD static inline bool parse_int(io::char_view value, io::i32& out_int) noexcept {
        const io::char_view t = trim_ascii(value);
        if (t.empty()) return false;

        io::usize i = 0;
        bool neg = false;
        if (t[i] == '+' || t[i] == '-') {
            neg = (t[i] == '-');
            ++i;
            if (i >= t.size()) return false;
        }

        io::u32 acc = 0u;
        const io::u32 limit = neg ? 2147483648u : 2147483647u;
        for (; i < t.size(); ++i) {
            const char ch = t[i];
            if (ch < '0' || ch > '9') return false;
            const io::u32 digit = static_cast<io::u32>(ch - '0');
            if (acc > (limit - digit) / 10u) return false;
            acc = acc * 10u + digit;
        }

        if (neg) {
            if (acc == 2147483648u) out_int = (-2147483647 - 1);
            else out_int = -static_cast<io::i32>(acc);
        } else {
            out_int = static_cast<io::i32>(acc);
        }
        return true;
    }

    IO_NODISCARD static inline bool parse_bool(io::char_view value, bool& out_bool) noexcept {
        const io::char_view t = trim_ascii(value);
        if (t.empty()) return false;
        if (iequals_ascii(t, "1") || iequals_ascii(t, "true") || iequals_ascii(t, "yes") || iequals_ascii(t, "on")) {
            out_bool = true;
            return true;
        }
        if (iequals_ascii(t, "0") || iequals_ascii(t, "false") || iequals_ascii(t, "no") || iequals_ascii(t, "off")) {
            out_bool = false;
            return true;
        }
        return false;
    }

    IO_NODISCARD static inline bool parse_string_max_64_bytes(io::char_view value,
                                                               char (&out_string)[64],
                                                               io::u32& out_size) noexcept {
        io::char_view t = trim_ascii(value);
        if (t.size() >= 2) {
            const char first = t[0];
            const char last = t[t.size() - 1];
            if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
                t = t.slice(1, t.size() - 2);
        }

        if (t.size() > 64u) return false;
        for (io::u32 i = 0; i < 64u; ++i) out_string[i] = '\0';
        for (io::usize i = 0; i < t.size(); ++i) out_string[i] = t[i];
        out_size = static_cast<io::u32>(t.size());
        return true;
    }

    static inline void set_default_server_text_config(ServerTextConfig& cfg) noexcept {
        cfg = {};
        cfg.job_threads = 0;
        cfg.distance_chunks = 10;
        cfg.hot_chunks = 1;
        cfg.day_time_multiply = 20;
    }

    IO_NODISCARD static inline io::i32 hot_radius_from_legacy_count(io::i32 legacy_count) noexcept {
        if (legacy_count <= 1) return 1;
        io::i32 r = 1;
        while (r < 16) {
            const io::i32 side = r * 2 + 1;
            const io::i32 cube = side * side * side;
            if (cube >= legacy_count) break;
            ++r;
        }
        return r;
    }

    IO_NODISCARD static inline bool sanitize_server_text_config(ServerTextConfig& cfg) noexcept {
        const io::i32 before_distance = cfg.distance_chunks;
        const io::i32 before = cfg.job_threads;
        const io::i32 before_hot = cfg.hot_chunks;
        const io::i32 before_day = cfg.day_time_multiply;
        if (cfg.job_threads > 4096) cfg.job_threads = 4096;
        if (cfg.distance_chunks < 1) cfg.distance_chunks = 1;
        if (cfg.distance_chunks > 16) cfg.distance_chunks = 16;
        if (cfg.hot_chunks > 16)
            cfg.hot_chunks = hot_radius_from_legacy_count(cfg.hot_chunks);
        if (cfg.hot_chunks < 1) cfg.hot_chunks = 1;
        if (cfg.hot_chunks > 16) cfg.hot_chunks = 16;
        if (cfg.hot_chunks > cfg.distance_chunks) cfg.hot_chunks = cfg.distance_chunks;
        if (cfg.day_time_multiply < 1) cfg.day_time_multiply = 1;
        if (cfg.day_time_multiply > 240) cfg.day_time_multiply = 240;
        return cfg.job_threads != before || cfg.distance_chunks != before_distance || cfg.hot_chunks != before_hot ||
               cfg.day_time_multiply != before_day;
    }

    IO_NODISCARD static inline bool build_server_config_text_path(io::string& out_path) noexcept {
        const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (fs::is_directory(roots[i]))
                return fs::path_join(roots[i], FILENAME_SERVER_CONFIG_TXT, out_path);
        }
        return fs::path_join(PATH_RESOURCES, FILENAME_SERVER_CONFIG_TXT, out_path);
    }

    IO_NODISCARD static inline bool write_server_text_config(io::char_view path,
                                                              const ServerTextConfig& cfg) noexcept {
        fs::File f{ path, io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Truncate };
        if (!f.is_open()) return false;

        io::StackOut<256> out{};
        out.reset();
        out << "# Grave Engine server config\n";
        out << "job_threads = " << cfg.job_threads << '\n';
        out << "distance_chunks = " << cfg.distance_chunks << '\n';
        out << "hot_chunks = " << cfg.hot_chunks << " # simulation radius (1 => 3x3x3)\n";
        out << "day-time-multiply = " << cfg.day_time_multiply << " # minutes\n";

        const io::char_view bytes = out.view();
        if (f.write(bytes) != bytes.size()) return false;
        return f.flush();
    }

    IO_NODISCARD static inline bool ensure_server_text_config(ServerTextConfig& out_cfg) noexcept {
        set_default_server_text_config(out_cfg);

        io::string path{};
        if (!build_server_config_text_path(path)) {
            out_cfg.state = ServerTextConfigState::Failed;
            return false;
        }

        if (!fs::exists(path)) {
            if (!write_server_text_config(path.as_view(), out_cfg)) {
                out_cfg.state = ServerTextConfigState::Failed;
                return false;
            }
            out_cfg.state = ServerTextConfigState::Created;
            return true;
        }

        fs::File f{ path.as_view(), io::OpenMode::Read };
        if (!f.is_open()) {
            out_cfg.state = ServerTextConfigState::Failed;
            return false;
        }

        bool seen_job_threads = false;
        bool seen_distance_chunks = false;
        bool seen_hot_chunks = false;
        bool seen_day_time_multiply = false;
        bool needs_rewrite = false;
        io::string line{};
        while (f.read_line(line)) {
            io::char_view value{};
            if (parse_config_field_value("job_threads", line.as_view(), value)) {
                io::i32 parsed = 0;
                if (!parse_int(value, parsed)) {
                    needs_rewrite = true;
                    continue;
                }
                out_cfg.job_threads = parsed;
                seen_job_threads = true;
            }
            if (parse_config_field_value("distance_chunks", line.as_view(), value)) {
                io::i32 parsed = 0;
                if (!parse_int(value, parsed)) {
                    needs_rewrite = true;
                    continue;
                }
                out_cfg.distance_chunks = parsed;
                seen_distance_chunks = true;
            }
            if (parse_config_field_value("hot_chunks", line.as_view(), value)) {
                io::i32 parsed = 0;
                if (!parse_int(value, parsed)) {
                    needs_rewrite = true;
                    continue;
                }
                out_cfg.hot_chunks = parsed;
                seen_hot_chunks = true;
            }
            if (parse_config_field_value("day-time-multiply", line.as_view(), value) ||
                parse_config_field_value("day_time_multiply", line.as_view(), value)) {
                io::i32 parsed = 0;
                if (!parse_int(value, parsed)) {
                    needs_rewrite = true;
                    continue;
                }
                out_cfg.day_time_multiply = parsed;
                seen_day_time_multiply = true;
            }
        }

        if (!seen_job_threads) {
            out_cfg.job_threads = 0;
            needs_rewrite = true;
        }
        if (!seen_distance_chunks) {
            out_cfg.distance_chunks = 10;
            needs_rewrite = true;
        }
        if (!seen_hot_chunks) {
            out_cfg.hot_chunks = 1;
            needs_rewrite = true;
        }
        if (!seen_day_time_multiply) {
            out_cfg.day_time_multiply = 20;
            needs_rewrite = true;
        }
        if (sanitize_server_text_config(out_cfg))
            needs_rewrite = true;

        if (needs_rewrite) {
            if (!write_server_text_config(path.as_view(), out_cfg)) {
                out_cfg.state = ServerTextConfigState::Failed;
                return false;
            }
            out_cfg.state = ServerTextConfigState::Updated;
            return true;
        }

        out_cfg.state = ServerTextConfigState::LoadedCurrent;
        return true;
    }
} // namespace ge
