#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"
#include "../../../3rd_party/hi/hi/hi.hpp"

namespace ge {
    // ------------------------------------------------------------------------
    // Versions
    // ------------------------------------------------------------------------
    static constexpr io::u32 ENGINE_VERSION = 1u;
    static constexpr io::u32 GAME_VERSION = 1u;

    struct ConfigHeader {
        io::u32 engine_version = ENGINE_VERSION;
        io::u32 game_version = GAME_VERSION;
    };

    struct ConfigClientData {
        char player_name_utf8[32]{};
        io::u32 render_distance_chunks = 4u;
        io::u32 mesh_workers = 4u;
        io::u32 extra_radius = 4u;
        bool is_dark_theme = true;
    };

    enum class ConfigState : io::u8 {
        LoadedCurrent = 0,
        Created,
        Updated,
        Failed
    };

    struct ConfigBinary {
        ConfigHeader header{};
        ConfigClientData client{};
        ConfigState state = ConfigState::Failed;
    };

    // ------------------------------------------------------------------------
    // Strings
    // ------------------------------------------------------------------------
    static io::char_view PATH_RESOURCES{ "../resources/" };
    static io::char_view PATH_RESOURCES_ALT_1{ "../../resources/" };
    static io::char_view PATH_RESOURCES_ALT_2{ "resources/" };
    static io::char_view PATH_SHADERS{ "shaders/" };
    static io::char_view PATH_TEXTURES{ "textures/" };
    static io::char_view PATH_FONTS_TTF{ "fonts ttf/" };

    static io::char_view WINDOW_TITLE{ "Grave Below" };

    static io::char_view FILENAME_LOG{ "log.txt" };
    static io::char_view FILENAME_WORLD_FONT{ "Monocraft.ttf" };
    static io::char_view FILENAME_CONFIG_BIN{ "config.bin" };
    static io::char_view FILENAME_KEY_BINDINGS_BIN{ "key_bindings.bin" };
    static io::char_view FILENAME_SERVER_LIST_BIN{ "server_list.bin" };
    static io::char_view DEFAULT_MULTIPLAYER_ENDPOINT{ "127.0.0.1:25565" };

    IO_NODISCARD static inline io::char_view config_state_to_string(ConfigState state) noexcept {
        switch (state) {
        case ConfigState::LoadedCurrent: return "loaded-current";
        case ConfigState::Created:       return "created";
        case ConfigState::Updated:       return "updated";
        default:                         return "failed";
        }
    }

    IO_NODISCARD static inline bool is_config_version_current(const ConfigHeader& header) noexcept {
        return (header.engine_version == ENGINE_VERSION) && (header.game_version == GAME_VERSION);
    }

    IO_NODISCARD static inline bool build_config_path(io::string& out_path) noexcept {
        const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (fs::is_directory(roots[i]))
                return fs::path_join(roots[i], FILENAME_CONFIG_BIN, out_path);
        }
        return fs::path_join(PATH_RESOURCES, FILENAME_CONFIG_BIN, out_path);
    }

    static inline void set_default_player_name(char (&out_name)[32]) noexcept {
        for (io::usize i = 0; i < 32; ++i) out_name[i] = '\0';
        static constexpr char k_default[] = "Player";
        for (io::usize i = 0; i < sizeof(k_default) - 1; ++i)
            out_name[i] = k_default[i];
    }

    static inline void sanitize_client_data(ConfigClientData& data) noexcept {
        bool has_any = false;
        for (io::usize i = 0; i < 32; ++i) {
            if (data.player_name_utf8[i] == '\n' || data.player_name_utf8[i] == '\r')
                data.player_name_utf8[i] = ' ';
            if (data.player_name_utf8[i] != '\0') has_any = true;
        }
        if (!has_any) set_default_player_name(data.player_name_utf8);

        if (data.render_distance_chunks < 1u) data.render_distance_chunks = 1u;
        if (data.render_distance_chunks > 16u) data.render_distance_chunks = 16u;
        if (data.mesh_workers < 1u) data.mesh_workers = 1u;
        if (data.mesh_workers > 4096u) data.mesh_workers = 4096u;
        if (data.extra_radius < 1u) data.extra_radius = 1u;
        if (data.extra_radius > 16u) data.extra_radius = 16u;
    }

    static inline void set_default_client_data(ConfigClientData& data) noexcept {
        data = {};
        set_default_player_name(data.player_name_utf8);
        data.render_distance_chunks = 4u;
        data.mesh_workers = 4u;
        data.extra_radius = 4u;
        data.is_dark_theme = true;
    }

    static inline void set_default_config(ConfigBinary& cfg) noexcept {
        cfg = {};
        set_default_client_data(cfg.client);
    }

    IO_NODISCARD static inline bool write_config_binary(io::char_view path,
                                                         const ConfigBinary& cfg,
                                                         bool truncate) noexcept {
        io::OpenMode mode = io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Binary;
        if (truncate) mode |= io::OpenMode::Truncate;

        fs::File f{ path, mode };
        if (!f.is_open()) return false;
        if (!f.seek(0, io::SeekWhence::Begin)) return false;

        const io::char_view hbytes{ reinterpret_cast<const char*>(&cfg.header), sizeof(ConfigHeader) };
        if (f.write(hbytes) != hbytes.size()) return false;

        const io::char_view cbytes{ reinterpret_cast<const char*>(&cfg.client), sizeof(ConfigClientData) };
        if (f.write(cbytes) != cbytes.size()) return false;
        return f.flush();
    }

    IO_NODISCARD static inline bool save_config_binary(const ConfigBinary& cfg) noexcept {
        io::string path{};
        if (!build_config_path(path)) return false;
        return write_config_binary(path.as_view(), cfg, true);
    }

    IO_NODISCARD static inline bool read_config_binary(io::char_view path,
                                                        ConfigHeader& out_header,
                                                        ConfigClientData& out_client,
                                                        bool& out_has_full_client,
                                                        io::usize& out_client_bytes) noexcept {
        fs::File f{ path, io::OpenMode::Read | io::OpenMode::Binary };
        if (!f.is_open()) return false;
        if (!f.read_exact(&out_header, sizeof(ConfigHeader))) return false;

        for (io::usize i = 0; i < sizeof(ConfigClientData); ++i)
            reinterpret_cast<char*>(&out_client)[i] = 0;

        out_client_bytes = f.read(io::view<char>{ reinterpret_cast<char*>(&out_client), sizeof(ConfigClientData) });
        out_has_full_client = (out_client_bytes == sizeof(ConfigClientData));
        return true;
    }

    IO_NODISCARD static inline bool ensure_config_binary(ConfigBinary& out_cfg) noexcept {
        set_default_config(out_cfg);

        io::string path{};
        if (!build_config_path(path)) {
            out_cfg.state = ConfigState::Failed;
            return false;
        }

        if (!fs::exists(path)) {
            if (!write_config_binary(path.as_view(), out_cfg, true)) {
                out_cfg.state = ConfigState::Failed;
                return false;
            }
            out_cfg.state = ConfigState::Created;
            return true;
        }

        ConfigHeader header{};
        ConfigClientData client{};
        bool has_full_client = false;
        io::usize client_bytes = 0;
        if (!read_config_binary(path.as_view(), header, client, has_full_client, client_bytes)) {
            set_default_config(out_cfg);
            if (!write_config_binary(path.as_view(), out_cfg, true)) {
                out_cfg.state = ConfigState::Failed;
                return false;
            }
            out_cfg.state = ConfigState::Updated;
            return true;
        }
        (void)client_bytes;

        bool needs_update = false;
        if (!is_config_version_current(header)) {
            set_default_config(out_cfg);
            needs_update = true;
        } else {
            out_cfg.header = header;
            out_cfg.client = client;
            if (!has_full_client && out_cfg.client.extra_radius == 0u)
                out_cfg.client.extra_radius = 4u;
            sanitize_client_data(out_cfg.client);
            if (!has_full_client) needs_update = true;
        }

        if (needs_update) {
            out_cfg.header = {};
            if (!write_config_binary(path.as_view(), out_cfg, true)) {
                out_cfg.state = ConfigState::Failed;
                return false;
            }
            out_cfg.state = ConfigState::Updated;
            return true;
        }

        out_cfg.state = ConfigState::LoadedCurrent;
        return true;
    }

    // ------------------------------------------------------------------------
    // GUI
    // ------------------------------------------------------------------------

    static constexpr hi::TextStyle BUTTON_STYLE_NORMAL{ 1.f,  1.f,  1.f,  1.f, true, 0.f, 0.f, 0.f, 1.f, /*.outline_px*/0.9f, /*.softness_px*/0.9f };
    static constexpr hi::TextStyle BUTTON_STYLE_HOVER{ 1.f,  1.f,  1.f,  1.f, false };
    static constexpr hi::TextStyle BUTTON_STYLE_ACTIVE{ 0.f,  0.f,  0.f,  1.f, true, 1.f, 1.f, 1.f, 1.f, /*.outline_px*/1.5f, /*.softness_px*/0.9f };

    static constexpr float FONT_PIXEL_HEIGHT = 24.f;

    // ------------------------------------------------------------------------
    // In-game
    // ------------------------------------------------------------------------

    static constexpr float LIQUID_TRANSPARENCY = 0.83f;

} // namespace ge
