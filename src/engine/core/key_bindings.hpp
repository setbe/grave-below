#pragma once

#include "config.hpp"

namespace ge {
    struct KeyBinding {
        hi::Key key = hi::Key::__NONE__;
        hi::Key maybe_second_key = hi::Key::__NONE__;
    };

    struct KeyBindingsHeader {
        io::u32 magic = 0x424B4247u; // "GBKB" in little-endian
        io::u32 format_version = 1u;
        io::u32 engine_version = ENGINE_VERSION;
        io::u32 game_version = GAME_VERSION;
        io::u32 action_count = 0u;
    };

    struct KeyBindingDisk {
        io::u16 key = 0u;
        io::u16 maybe_second_key = 0u;
    };

    enum class KeyBindingsState : io::u8 {
        LoadedCurrent = 0,
        Created,
        Updated,
        Failed
    };

    struct KeyBindingsBinary {
        KeyBindingsHeader header{};
        KeyBindingsState state = KeyBindingsState::Failed;
    };

    IO_NODISCARD static inline io::char_view key_bindings_state_to_string(KeyBindingsState state) noexcept {
        switch (state) {
        case KeyBindingsState::LoadedCurrent: return "loaded-current";
        case KeyBindingsState::Created: return "created";
        case KeyBindingsState::Updated: return "updated";
        default: return "failed";
        }
    }

    IO_NODISCARD static inline bool build_key_bindings_path(io::string& out_path) noexcept {
        const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (fs::is_directory(roots[i]))
                return fs::path_join(roots[i], FILENAME_KEY_BINDINGS_BIN, out_path);
        }
        return fs::path_join(PATH_RESOURCES, FILENAME_KEY_BINDINGS_BIN, out_path);
    }

    IO_NODISCARD static inline hi::Key key_from_disk(io::u16 value) noexcept {
        if (value < static_cast<io::u16>(hi::Key::__LAST__))
            return static_cast<hi::Key>(value);
        return hi::Key::__NONE__;
    }

    IO_NODISCARD static inline io::u16 key_to_disk(hi::Key key) noexcept {
        const io::u32 value = static_cast<io::u32>(key);
        if (value < static_cast<io::u32>(hi::Key::__LAST__))
            return static_cast<io::u16>(value);
        return 0u;
    }

    static inline void copy_default_key_bindings(io::view<KeyBinding> out_bindings,
                                                 io::view<const KeyBinding> defaults) noexcept {
        const io::usize count = out_bindings.size();
        for (io::usize i = 0; i < count; ++i) {
            if (i < defaults.size()) out_bindings[i] = defaults[i];
            else out_bindings[i] = {};
        }
    }

    IO_NODISCARD static inline bool write_key_bindings_binary(io::char_view path,
                                                               io::view<const KeyBinding> bindings) noexcept {
        KeyBindingsHeader header{};
        header.action_count = static_cast<io::u32>(bindings.size());

        fs::File f{ path, io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Binary | io::OpenMode::Truncate };
        if (!f.is_open()) return false;

        const io::char_view hbytes{ reinterpret_cast<const char*>(&header), sizeof(header) };
        if (f.write(hbytes) != hbytes.size()) return false;

        for (io::usize i = 0; i < bindings.size(); ++i) {
            const KeyBindingDisk d{
                key_to_disk(bindings[i].key),
                key_to_disk(bindings[i].maybe_second_key)
            };
            const io::char_view dbytes{ reinterpret_cast<const char*>(&d), sizeof(d) };
            if (f.write(dbytes) != dbytes.size()) return false;
        }
        return f.flush();
    }

    IO_NODISCARD static inline bool save_key_bindings_binary(io::view<const KeyBinding> bindings) noexcept {
        io::string path{};
        if (!build_key_bindings_path(path)) return false;
        return write_key_bindings_binary(path.as_view(), bindings);
    }

    IO_NODISCARD static inline bool ensure_key_bindings_binary(io::view<KeyBinding> out_bindings,
                                                                io::view<const KeyBinding> defaults,
                                                                KeyBindingsBinary& out_meta) noexcept {
        out_meta = {};
        if (out_bindings.empty()) {
            out_meta.state = KeyBindingsState::Failed;
            return false;
        }

        copy_default_key_bindings(out_bindings, defaults);

        io::string path{};
        if (!build_key_bindings_path(path)) {
            out_meta.state = KeyBindingsState::Failed;
            return false;
        }

        if (!fs::exists(path)) {
            if (!write_key_bindings_binary(path.as_view(), io::view<const KeyBinding>{ out_bindings.data(), out_bindings.size() })) {
                out_meta.state = KeyBindingsState::Failed;
                return false;
            }
            out_meta.header.action_count = static_cast<io::u32>(out_bindings.size());
            out_meta.state = KeyBindingsState::Created;
            return true;
        }

        fs::File f{ path.as_view(), io::OpenMode::Read | io::OpenMode::Binary };
        if (!f.is_open()) {
            out_meta.state = KeyBindingsState::Failed;
            return false;
        }

        KeyBindingsHeader file_header{};
        if (!f.read_exact(&file_header, sizeof(file_header)) ||
            file_header.magic != 0x424B4247u ||
            file_header.format_version != 1u) {
            if (!write_key_bindings_binary(path.as_view(), io::view<const KeyBinding>{ out_bindings.data(), out_bindings.size() })) {
                out_meta.state = KeyBindingsState::Failed;
                return false;
            }
            out_meta.header.action_count = static_cast<io::u32>(out_bindings.size());
            out_meta.state = KeyBindingsState::Updated;
            return true;
        }

        const io::usize known_count = out_bindings.size();
        const io::usize file_count = static_cast<io::usize>(file_header.action_count);
        const io::usize load_count = (file_count < known_count) ? file_count : known_count;

        for (io::usize i = 0; i < load_count; ++i) {
            KeyBindingDisk d{};
            if (!f.read_exact(&d, sizeof(d))) {
                copy_default_key_bindings(out_bindings, defaults);
                if (!write_key_bindings_binary(path.as_view(), io::view<const KeyBinding>{ out_bindings.data(), out_bindings.size() })) {
                    out_meta.state = KeyBindingsState::Failed;
                    return false;
                }
                out_meta.header.action_count = static_cast<io::u32>(out_bindings.size());
                out_meta.state = KeyBindingsState::Updated;
                return true;
            }
            out_bindings[i].key = key_from_disk(d.key);
            out_bindings[i].maybe_second_key = key_from_disk(d.maybe_second_key);
            if (out_bindings[i].key == hi::Key::__NONE__)
                out_bindings[i] = (i < defaults.size()) ? defaults[i] : KeyBinding{};
        }

        bool should_rewrite = false;
        if (file_header.action_count < static_cast<io::u32>(known_count))
            should_rewrite = true;
        if (file_header.engine_version != ENGINE_VERSION || file_header.game_version != GAME_VERSION)
            should_rewrite = true;

        if (should_rewrite) {
            if (!write_key_bindings_binary(path.as_view(), io::view<const KeyBinding>{ out_bindings.data(), out_bindings.size() })) {
                out_meta.state = KeyBindingsState::Failed;
                return false;
            }
            out_meta.header.action_count = static_cast<io::u32>(out_bindings.size());
            out_meta.state = KeyBindingsState::Updated;
            return true;
        }

        out_meta.header = file_header;
        out_meta.state = KeyBindingsState::LoadedCurrent;
        return true;
    }
} // namespace ge
