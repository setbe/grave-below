#pragma once

#include "config.hpp"

namespace ge {
    struct ServerListEntry {
        char name_utf8[32]{};
        char ip_utf8[48]{};
        io::u16 port = 25565u;
        io::u16 _pad = 0u;
    };

    struct ServerListHeader {
        io::u32 magic = 0x4C534247u; // "GBSL"
        io::u32 format_version = 1u;
        io::u32 engine_version = ENGINE_VERSION;
        io::u32 game_version = GAME_VERSION;
        io::u32 server_count = 0u;
    };

    enum class ServerListState : io::u8 {
        LoadedCurrent = 0,
        Created,
        Updated,
        Failed
    };

    struct ServerListBinary {
        ServerListHeader header{};
        ServerListState state = ServerListState::Failed;
    };

    IO_NODISCARD static inline io::char_view server_list_state_to_string(ServerListState state) noexcept {
        switch (state) {
        case ServerListState::LoadedCurrent: return "loaded-current";
        case ServerListState::Created:       return "created";
        case ServerListState::Updated:       return "updated";
        default:                             return "failed";
        }
    }

    static inline void set_default_server_entry(ServerListEntry& out_entry) noexcept {
        out_entry = {};
        static constexpr char k_name[] = "Localhost";
        static constexpr char k_ip[] = "127.0.0.1";
        for (io::usize i = 0; i < sizeof(k_name) - 1; ++i)
            out_entry.name_utf8[i] = k_name[i];
        for (io::usize i = 0; i < sizeof(k_ip) - 1; ++i)
            out_entry.ip_utf8[i] = k_ip[i];
        out_entry.port = 25565u;
    }

    static inline void sanitize_server_entry(ServerListEntry& entry) noexcept {
        bool has_name = false;
        bool has_ip = false;

        for (io::usize i = 0; i < sizeof(entry.name_utf8); ++i) {
            if (entry.name_utf8[i] == '\n' || entry.name_utf8[i] == '\r')
                entry.name_utf8[i] = ' ';
            if (entry.name_utf8[i] != '\0')
                has_name = true;
        }
        for (io::usize i = 0; i < sizeof(entry.ip_utf8); ++i) {
            if (entry.ip_utf8[i] == '\n' || entry.ip_utf8[i] == '\r')
                entry.ip_utf8[i] = ' ';
            if (entry.ip_utf8[i] != '\0')
                has_ip = true;
        }

        if (!has_name) {
            static constexpr char k_name[] = "Server";
            for (io::usize i = 0; i < sizeof(entry.name_utf8); ++i)
                entry.name_utf8[i] = '\0';
            for (io::usize i = 0; i < sizeof(k_name) - 1; ++i)
                entry.name_utf8[i] = k_name[i];
        }
        if (!has_ip) {
            static constexpr char k_ip[] = "127.0.0.1";
            for (io::usize i = 0; i < sizeof(entry.ip_utf8); ++i)
                entry.ip_utf8[i] = '\0';
            for (io::usize i = 0; i < sizeof(k_ip) - 1; ++i)
                entry.ip_utf8[i] = k_ip[i];
        }
        if (entry.port == 0u) entry.port = 25565u;
    }

    static inline void set_default_server_list(io::vector<ServerListEntry>& out_entries) noexcept {
        out_entries.clear();
        if (!out_entries.resize(1))
            return;
        set_default_server_entry(out_entries[0]);
    }

    IO_NODISCARD static inline bool build_server_list_path(io::string& out_path) noexcept {
        const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (fs::is_directory(roots[i]))
                return fs::path_join(roots[i], FILENAME_SERVER_LIST_BIN, out_path);
        }
        return fs::path_join(PATH_RESOURCES, FILENAME_SERVER_LIST_BIN, out_path);
    }

    IO_NODISCARD static inline bool write_server_list_binary(io::char_view path,
                                                             io::view<const ServerListEntry> entries) noexcept {
        ServerListHeader header{};
        header.server_count = static_cast<io::u32>(entries.size());

        fs::File f{ path, io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Binary | io::OpenMode::Truncate };
        if (!f.is_open()) return false;

        const io::char_view hbytes{ reinterpret_cast<const char*>(&header), sizeof(header) };
        if (f.write(hbytes) != hbytes.size()) return false;

        for (io::usize i = 0; i < entries.size(); ++i) {
            ServerListEntry e = entries[i];
            sanitize_server_entry(e);
            const io::char_view ebytes{ reinterpret_cast<const char*>(&e), sizeof(e) };
            if (f.write(ebytes) != ebytes.size()) return false;
        }

        return f.flush();
    }

    IO_NODISCARD static inline bool save_server_list_binary(io::view<const ServerListEntry> entries) noexcept {
        io::string path{};
        if (!build_server_list_path(path)) return false;
        return write_server_list_binary(path.as_view(), entries);
    }

    IO_NODISCARD static inline bool ensure_server_list_binary(io::vector<ServerListEntry>& out_entries,
                                                              ServerListBinary& out_meta) noexcept {
        out_meta = {};
        out_entries.clear();

        io::string path{};
        if (!build_server_list_path(path)) {
            out_meta.state = ServerListState::Failed;
            return false;
        }

        if (!fs::exists(path)) {
            set_default_server_list(out_entries);
            if (out_entries.empty() ||
                !write_server_list_binary(path.as_view(),
                                          io::view<const ServerListEntry>{ out_entries.data(), out_entries.size() })) {
                out_meta.state = ServerListState::Failed;
                return false;
            }
            out_meta.header.server_count = static_cast<io::u32>(out_entries.size());
            out_meta.state = ServerListState::Created;
            return true;
        }

        fs::File f{ path.as_view(), io::OpenMode::Read | io::OpenMode::Binary };
        if (!f.is_open()) {
            out_meta.state = ServerListState::Failed;
            return false;
        }

        ServerListHeader file_header{};
        if (!f.read_exact(&file_header, sizeof(file_header)) ||
            file_header.magic != 0x4C534247u ||
            file_header.format_version != 1u) {
            set_default_server_list(out_entries);
            if (out_entries.empty() ||
                !write_server_list_binary(path.as_view(),
                                          io::view<const ServerListEntry>{ out_entries.data(), out_entries.size() })) {
                out_meta.state = ServerListState::Failed;
                return false;
            }
            out_meta.header.server_count = static_cast<io::u32>(out_entries.size());
            out_meta.state = ServerListState::Updated;
            return true;
        }

        const io::u32 max_entries = 512u;
        io::u32 file_count = file_header.server_count;
        if (file_count > max_entries) file_count = max_entries;

        if (file_count > 0u) {
            if (!out_entries.resize(static_cast<io::usize>(file_count))) {
                out_meta.state = ServerListState::Failed;
                return false;
            }
            for (io::u32 i = 0; i < file_count; ++i) {
                if (!f.read_exact(&out_entries[i], sizeof(ServerListEntry))) {
                    set_default_server_list(out_entries);
                    if (out_entries.empty() ||
                        !write_server_list_binary(path.as_view(),
                                                  io::view<const ServerListEntry>{ out_entries.data(), out_entries.size() })) {
                        out_meta.state = ServerListState::Failed;
                        return false;
                    }
                    out_meta.header.server_count = static_cast<io::u32>(out_entries.size());
                    out_meta.state = ServerListState::Updated;
                    return true;
                }
                sanitize_server_entry(out_entries[i]);
            }
        }

        if (out_entries.empty())
            set_default_server_list(out_entries);

        bool should_rewrite = false;
        if (file_header.engine_version != ENGINE_VERSION || file_header.game_version != GAME_VERSION)
            should_rewrite = true;
        if (file_header.server_count != static_cast<io::u32>(out_entries.size()))
            should_rewrite = true;

        if (should_rewrite) {
            if (!write_server_list_binary(path.as_view(),
                                          io::view<const ServerListEntry>{ out_entries.data(), out_entries.size() })) {
                out_meta.state = ServerListState::Failed;
                return false;
            }
            out_meta.header.server_count = static_cast<io::u32>(out_entries.size());
            out_meta.state = ServerListState::Updated;
            return true;
        }

        out_meta.header = file_header;
        out_meta.header.server_count = static_cast<io::u32>(out_entries.size());
        out_meta.state = ServerListState::LoadedCurrent;
        return true;
    }
} // namespace ge
