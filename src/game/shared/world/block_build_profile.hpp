#pragma once

#include "hi/hi/hi.hpp"

#include "../../../engine/core/config.hpp"
#include "../../../engine/voxel/block.hpp"

namespace ge {
namespace build {
    enum class BlockHardness : io::u8 {
        Light = 0u,
        Medium = 1u,
        Hard = 2u
    };

    enum class BlockBuildConfigState : io::u8 {
        LoadedCurrent = 0u,
        Created = 1u,
        Updated = 2u,
        Failed = 3u
    };

    struct BlockBuildInfo {
        BlockHardness hardness = BlockHardness::Medium;
        bool transparent = false;
        char tex_side[32]{};
        char tex_top[32]{};
        char tex_bottom[32]{};
    };

    struct BlockBuildProfile {
        BlockBuildInfo blocks[ge::voxel::BLOCK_COUNT]{};
        BlockBuildConfigState state = BlockBuildConfigState::Failed;
    };

    static io::char_view FILENAME_BUILD_CONFIG_YAML{ "build.yaml" };

    IO_NODISCARD static inline io::char_view block_build_config_state_to_string(BlockBuildConfigState state) noexcept {
        switch (state) {
        case BlockBuildConfigState::LoadedCurrent: return "loaded-current";
        case BlockBuildConfigState::Created:       return "created";
        case BlockBuildConfigState::Updated:       return "updated";
        default:                                   return "failed";
        }
    }

    static inline bool bb_is_ascii_space(char ch) noexcept {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }

    IO_NODISCARD static inline io::char_view bb_trim_ascii(io::char_view value) noexcept {
        io::usize begin = 0u;
        io::usize end = value.size();
        while (begin < end && bb_is_ascii_space(value[begin])) ++begin;
        while (end > begin && bb_is_ascii_space(value[end - 1u])) --end;
        if (end <= begin) return {};
        return value.slice(begin, end - begin);
    }

    static inline char bb_ascii_lower(char ch) noexcept {
        if (ch >= 'A' && ch <= 'Z') return static_cast<char>(ch - 'A' + 'a');
        return ch;
    }

    IO_NODISCARD static inline bool bb_iequals_ascii(io::char_view a, io::char_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (io::usize i = 0u; i < a.size(); ++i)
            if (bb_ascii_lower(a[i]) != bb_ascii_lower(b[i])) return false;
        return true;
    }

    IO_NODISCARD static inline bool bb_parse_bool(io::char_view value, bool& out_bool) noexcept {
        const io::char_view t = bb_trim_ascii(value);
        if (t.empty()) return false;
        if (bb_iequals_ascii(t, "1") || bb_iequals_ascii(t, "true") || bb_iequals_ascii(t, "yes") || bb_iequals_ascii(t, "on")) {
            out_bool = true;
            return true;
        }
        if (bb_iequals_ascii(t, "0") || bb_iequals_ascii(t, "false") || bb_iequals_ascii(t, "no") || bb_iequals_ascii(t, "off")) {
            out_bool = false;
            return true;
        }
        return false;
    }

    static inline bool bb_copy_string_31(io::char_view in, char (&out)[32]) noexcept {
        const io::char_view t = bb_trim_ascii(in);
        if (t.size() >= 32u) return false;
        for (io::u32 i = 0u; i < 32u; ++i) out[i] = '\0';
        for (io::usize i = 0u; i < t.size(); ++i) out[i] = t[i];
        return true;
    }

    static inline bool bb_write_kv_line(fs::File& f, io::char_view key, io::char_view value) noexcept {
        io::StackOut<256> line{};
        line << key << " = " << value << "\n";
        const io::char_view bytes = line.view();
        return f.write(bytes) == bytes.size();
    }

    static inline io::char_view bb_block_key_name(ge::voxel::BlockId id) noexcept {
        switch (id) {
        case ge::voxel::BlockId::Air: return "air";
        case ge::voxel::BlockId::Grass: return "grass";
        case ge::voxel::BlockId::Dirt: return "dirt";
        case ge::voxel::BlockId::Stone: return "stone";
        case ge::voxel::BlockId::Sand: return "sand";
        case ge::voxel::BlockId::Water: return "water";
        case ge::voxel::BlockId::Blood: return "blood";
        case ge::voxel::BlockId::Slime: return "slime";
        case ge::voxel::BlockId::Snow: return "snow";
        case ge::voxel::BlockId::GrassPale: return "grass_pale";
        case ge::voxel::BlockId::DirtDry: return "dirt_dry";
        case ge::voxel::BlockId::StoneCracked: return "hardened_stone";
        case ge::voxel::BlockId::SandAsh: return "sand_ash";
        case ge::voxel::BlockId::WaterDark: return "water_dark";
        case ge::voxel::BlockId::BloodDark: return "blood_dark";
        case ge::voxel::BlockId::SlimeDark: return "slime_dark";
        case ge::voxel::BlockId::SnowDirty: return "snow_dirty";
        case ge::voxel::BlockId::LevitatingBookAnchor: return "book_anchor";
        case ge::voxel::BlockId::Log: return "log";
        case ge::voxel::BlockId::Leaves: return "leaves";
        default: return "unknown";
        }
    }

    IO_NODISCARD static inline bool bb_match_token_loosely(io::char_view token, io::char_view key) noexcept {
        io::usize i = 0u;
        io::usize j = 0u;
        while (i < token.size() || j < key.size()) {
            while (i < token.size() && (token[i] == '_' || token[i] == '-' || token[i] == ' ' || token[i] == '.')) ++i;
            while (j < key.size() && (key[j] == '_' || key[j] == '-' || key[j] == ' ' || key[j] == '.')) ++j;
            if (i >= token.size() || j >= key.size())
                break;
            if (bb_ascii_lower(token[i]) != bb_ascii_lower(key[j]))
                return false;
            ++i;
            ++j;
        }
        while (i < token.size() && (token[i] == '_' || token[i] == '-' || token[i] == ' ' || token[i] == '.')) ++i;
        while (j < key.size() && (key[j] == '_' || key[j] == '-' || key[j] == ' ' || key[j] == '.')) ++j;
        return i == token.size() && j == key.size();
    }

    IO_NODISCARD static inline bool bb_parse_hardness(io::char_view value, BlockHardness& out) noexcept {
        const io::char_view t = bb_trim_ascii(value);
        if (bb_match_token_loosely(t, "light")) { out = BlockHardness::Light; return true; }
        if (bb_match_token_loosely(t, "medium")) { out = BlockHardness::Medium; return true; }
        if (bb_match_token_loosely(t, "hard")) { out = BlockHardness::Hard; return true; }
        return false;
    }

    IO_NODISCARD static inline io::char_view bb_hardness_name(BlockHardness h) noexcept {
        switch (h) {
        case BlockHardness::Light: return "light";
        case BlockHardness::Hard: return "hard";
        default: return "medium";
        }
    }

    IO_NODISCARD static inline bool bb_parse_block_name(io::char_view token, ge::voxel::BlockId& out_id) noexcept {
        const io::char_view t = bb_trim_ascii(token);
        if (t.empty()) return false;
        if (bb_match_token_loosely(t, "air")) { out_id = ge::voxel::BlockId::Air; return true; }
        if (bb_match_token_loosely(t, "grass")) { out_id = ge::voxel::BlockId::Grass; return true; }
        if (bb_match_token_loosely(t, "dirt")) { out_id = ge::voxel::BlockId::Dirt; return true; }
        if (bb_match_token_loosely(t, "stone") || bb_match_token_loosely(t, "cobblestone")) { out_id = ge::voxel::BlockId::Stone; return true; }
        if (bb_match_token_loosely(t, "sand")) { out_id = ge::voxel::BlockId::Sand; return true; }
        if (bb_match_token_loosely(t, "water")) { out_id = ge::voxel::BlockId::Water; return true; }
        if (bb_match_token_loosely(t, "blood")) { out_id = ge::voxel::BlockId::Blood; return true; }
        if (bb_match_token_loosely(t, "slime")) { out_id = ge::voxel::BlockId::Slime; return true; }
        if (bb_match_token_loosely(t, "snow")) { out_id = ge::voxel::BlockId::Snow; return true; }
        if (bb_match_token_loosely(t, "grass_pale")) { out_id = ge::voxel::BlockId::GrassPale; return true; }
        if (bb_match_token_loosely(t, "dirt_dry")) { out_id = ge::voxel::BlockId::DirtDry; return true; }
        if (bb_match_token_loosely(t, "hardened_stone") || bb_match_token_loosely(t, "stone_cracked")) {
            out_id = ge::voxel::BlockId::StoneCracked; return true;
        }
        if (bb_match_token_loosely(t, "sand_ash")) { out_id = ge::voxel::BlockId::SandAsh; return true; }
        if (bb_match_token_loosely(t, "water_dark")) { out_id = ge::voxel::BlockId::WaterDark; return true; }
        if (bb_match_token_loosely(t, "blood_dark")) { out_id = ge::voxel::BlockId::BloodDark; return true; }
        if (bb_match_token_loosely(t, "slime_dark")) { out_id = ge::voxel::BlockId::SlimeDark; return true; }
        if (bb_match_token_loosely(t, "snow_dirty")) { out_id = ge::voxel::BlockId::SnowDirty; return true; }
        if (bb_match_token_loosely(t, "book_anchor") || bb_match_token_loosely(t, "book") || bb_match_token_loosely(t, "levitating_book_anchor")) {
            out_id = ge::voxel::BlockId::LevitatingBookAnchor; return true;
        }
        if (bb_match_token_loosely(t, "log")) { out_id = ge::voxel::BlockId::Log; return true; }
        if (bb_match_token_loosely(t, "leaves")) { out_id = ge::voxel::BlockId::Leaves; return true; }
        return false;
    }

    static inline void set_default_block_build_profile(BlockBuildProfile& out_profile) noexcept {
        out_profile = {};
        for (io::u16 i = 0u; i < ge::voxel::BLOCK_COUNT; ++i) {
            const ge::voxel::BlockId id = static_cast<ge::voxel::BlockId>(i);
            BlockBuildInfo& info = out_profile.blocks[i];
            info.hardness = BlockHardness::Medium;
            info.transparent = ge::voxel::is_transparent(id);
            (void)bb_copy_string_31(bb_block_key_name(id), info.tex_side);
            (void)bb_copy_string_31(bb_block_key_name(id), info.tex_top);
            (void)bb_copy_string_31(bb_block_key_name(id), info.tex_bottom);
        }

        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Air)].hardness = BlockHardness::Hard;
        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Grass)].hardness = BlockHardness::Light;
        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Dirt)].hardness = BlockHardness::Light;
        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Sand)].hardness = BlockHardness::Light;
        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Log)].hardness = BlockHardness::Light;
        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Leaves)].hardness = BlockHardness::Light;
        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Stone)].hardness = BlockHardness::Hard;
        out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::StoneCracked)].hardness = BlockHardness::Hard;

        // Better defaults for face-composed blocks.
        (void)bb_copy_string_31("grass_side", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Grass)].tex_side);
        (void)bb_copy_string_31("grass_up", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Grass)].tex_top);
        (void)bb_copy_string_31("dirt", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Grass)].tex_bottom);
        (void)bb_copy_string_31("log-side", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Log)].tex_side);
        (void)bb_copy_string_31("log", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Log)].tex_top);
        (void)bb_copy_string_31("log", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::Log)].tex_bottom);
        (void)bb_copy_string_31("hardened_stone", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::StoneCracked)].tex_side);
        (void)bb_copy_string_31("hardened_stone", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::StoneCracked)].tex_top);
        (void)bb_copy_string_31("hardened_stone", out_profile.blocks[ge::voxel::block_index(ge::voxel::BlockId::StoneCracked)].tex_bottom);
    }

    IO_NODISCARD static inline bool build_block_profile_path(io::string& out_path) noexcept {
        const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0u; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (fs::is_directory(roots[i]))
                return fs::path_join(roots[i], FILENAME_BUILD_CONFIG_YAML, out_path);
        }
        return fs::path_join(PATH_RESOURCES, FILENAME_BUILD_CONFIG_YAML, out_path);
    }

    IO_NODISCARD static inline bool write_block_build_profile(io::char_view path,
                                                               const BlockBuildProfile& profile) noexcept {
        fs::File f{ path, io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Truncate };
        if (!f.is_open()) return false;

        io::StackOut<256> header{};
        header << "# Grave Below block build profile\n";
        header << "# Format: block.<name>.<property> = <value>\n";
        header << "# Properties: hardness(light|medium|hard), transparent(bool), texture.side/top/bottom\n";
        const io::char_view h = header.view();
        if (f.write(h) != h.size()) return false;

        for (io::u16 i = 0u; i < ge::voxel::BLOCK_COUNT; ++i) {
            const ge::voxel::BlockId id = static_cast<ge::voxel::BlockId>(i);
            const io::char_view key = bb_block_key_name(id);
            const BlockBuildInfo& info = profile.blocks[i];

            io::StackOut<96> k{};
            k << "block." << key << ".hardness";
            if (!bb_write_kv_line(f, k.view(), bb_hardness_name(info.hardness))) return false;

            k.reset();
            k << "block." << key << ".transparent";
            if (!bb_write_kv_line(f, k.view(), info.transparent ? "true" : "false")) return false;

            if (info.tex_side[0] != '\0') {
                k.reset();
                k << "block." << key << ".texture.side";
                if (!bb_write_kv_line(f, k.view(), io::char_view{ info.tex_side })) return false;
            }
            if (info.tex_top[0] != '\0') {
                k.reset();
                k << "block." << key << ".texture.top";
                if (!bb_write_kv_line(f, k.view(), io::char_view{ info.tex_top })) return false;
            }
            if (info.tex_bottom[0] != '\0') {
                k.reset();
                k << "block." << key << ".texture.bottom";
                if (!bb_write_kv_line(f, k.view(), io::char_view{ info.tex_bottom })) return false;
            }
        }
        return f.flush();
    }

    IO_NODISCARD static inline bool parse_block_build_kv(io::char_view line,
                                                          io::char_view& out_key,
                                                          io::char_view& out_value) noexcept {
        out_key = {};
        out_value = {};
        io::char_view t = bb_trim_ascii(line);
        if (t.empty()) return false;
        if (t[0] == '#' || t[0] == ';') return false;
        if (t.size() >= 2u && t[0] == '/' && t[1] == '/') return false;

        io::usize eq = io::npos;
        for (io::usize i = 0u; i < t.size(); ++i) {
            if (t[i] == '=') {
                eq = i;
                break;
            }
        }
        if (eq == io::npos) return false;

        out_key = bb_trim_ascii(t.slice(0u, eq));
        out_value = bb_trim_ascii(t.slice(eq + 1u, t.size() - (eq + 1u)));
        if (out_key.empty() || out_value.empty()) return false;

        io::usize cut = out_value.size();
        for (io::usize i = 0u; i < out_value.size(); ++i) {
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
        out_value = bb_trim_ascii(out_value.slice(0u, cut));
        return !out_value.empty();
    }

    IO_NODISCARD static inline bool parse_block_build_key(io::char_view key,
                                                           ge::voxel::BlockId& out_id,
                                                           io::char_view& out_property) noexcept {
        out_property = {};
        key = bb_trim_ascii(key);
        static const io::char_view prefix{ "block." };
        if (key.size() <= prefix.size()) return false;
        for (io::usize i = 0u; i < prefix.size(); ++i)
            if (bb_ascii_lower(key[i]) != prefix[i])
                return false;

        io::char_view rest = key.slice(prefix.size(), key.size() - prefix.size());
        io::usize dot = io::npos;
        for (io::usize i = 0u; i < rest.size(); ++i) {
            if (rest[i] == '.') {
                dot = i;
                break;
            }
        }
        if (dot == io::npos || dot == 0u || dot + 1u >= rest.size())
            return false;

        const io::char_view block_name = rest.slice(0u, dot);
        if (!bb_parse_block_name(block_name, out_id))
            return false;
        out_property = bb_trim_ascii(rest.slice(dot + 1u, rest.size() - (dot + 1u)));
        return !out_property.empty();
    }

    IO_NODISCARD static inline bool ensure_block_build_profile(BlockBuildProfile& out_profile) noexcept {
        set_default_block_build_profile(out_profile);

        io::string path{};
        if (!build_block_profile_path(path)) {
            out_profile.state = BlockBuildConfigState::Failed;
            return false;
        }

        if (!fs::exists(path)) {
            if (!write_block_build_profile(path.as_view(), out_profile)) {
                out_profile.state = BlockBuildConfigState::Failed;
                return false;
            }
            out_profile.state = BlockBuildConfigState::Created;
            return true;
        }

        fs::File f{ path.as_view(), io::OpenMode::Read };
        if (!f.is_open()) {
            out_profile.state = BlockBuildConfigState::Failed;
            return false;
        }

        bool seen_any = false;
        bool needs_rewrite = false;
        io::string line{};
        while (f.read_line(line)) {
            io::char_view key{};
            io::char_view value{};
            if (!parse_block_build_kv(line.as_view(), key, value))
                continue;

            ge::voxel::BlockId block_id = ge::voxel::BlockId::Air;
            io::char_view prop{};
            if (!parse_block_build_key(key, block_id, prop)) {
                needs_rewrite = true;
                continue;
            }

            seen_any = true;
            BlockBuildInfo& info = out_profile.blocks[ge::voxel::block_index(block_id)];
            if (bb_match_token_loosely(prop, "hardness")) {
                BlockHardness h = BlockHardness::Medium;
                if (!bb_parse_hardness(value, h)) {
                    needs_rewrite = true;
                    continue;
                }
                info.hardness = h;
            } else if (bb_match_token_loosely(prop, "transparent")) {
                bool b = false;
                if (!bb_parse_bool(value, b)) {
                    needs_rewrite = true;
                    continue;
                }
                info.transparent = b;
            } else if (bb_match_token_loosely(prop, "texture.side")) {
                if (!bb_copy_string_31(value, info.tex_side))
                    needs_rewrite = true;
            } else if (bb_match_token_loosely(prop, "texture.top")) {
                if (!bb_copy_string_31(value, info.tex_top))
                    needs_rewrite = true;
            } else if (bb_match_token_loosely(prop, "texture.bottom")) {
                if (!bb_copy_string_31(value, info.tex_bottom))
                    needs_rewrite = true;
            } else {
                needs_rewrite = true;
            }
        }

        if (!seen_any) needs_rewrite = true;

        if (needs_rewrite) {
            if (!write_block_build_profile(path.as_view(), out_profile)) {
                out_profile.state = BlockBuildConfigState::Failed;
                return false;
            }
            out_profile.state = BlockBuildConfigState::Updated;
            return true;
        }

        out_profile.state = BlockBuildConfigState::LoadedCurrent;
        return true;
    }

    IO_NODISCARD static inline BlockHardness hardness_of(const BlockBuildProfile& profile,
                                                         ge::voxel::BlockId id) noexcept {
        const io::u16 idx = ge::voxel::block_index(id);
        if (idx >= ge::voxel::BLOCK_COUNT)
            return BlockHardness::Hard;
        return profile.blocks[idx].hardness;
    }

    IO_NODISCARD static inline bool dagger_can_break(const BlockBuildProfile& profile,
                                                     ge::voxel::BlockId id) noexcept {
        if (id == ge::voxel::BlockId::Air) return false;
        return hardness_of(profile, id) == BlockHardness::Light;
    }
} // namespace build
} // namespace ge
