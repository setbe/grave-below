#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"
#include "../../../3rd_party/hi/hi/gl_loader.hpp"

#if !defined(IO_HAS_STD)
#   ifndef STBI_NO_JPEG
#       define STBI_NO_JPEG
#   endif
#   ifndef STBI_NO_BMP
#       define STBI_NO_BMP
#   endif
#   ifndef STBI_NO_GIF
#       define STBI_NO_GIF
#   endif
#   ifndef STBI_NO_PSD
#       define STBI_NO_PSD
#   endif
#   ifndef STBI_NO_PIC
#       define STBI_NO_PIC
#   endif
#   ifndef STBI_NO_PNM
#       define STBI_NO_PNM
#   endif
#   ifndef STBI_NO_HDR
#       define STBI_NO_HDR
#   endif
#   ifndef STBI_NO_TGA
#       define STBI_NO_TGA
#   endif
#endif

#include "../../../3rd_party/hi/3rd_party/stb/stb_image/stb_image.hpp"

#include "config.hpp"
#include "logger.hpp"

namespace ge {
    struct ResourceManager {
        static constexpr io::u32 INVALID_ID = 0xFFFFFFFFu;
        static constexpr io::u32 FACE_COUNT = 6u;

        struct NameIdEntry {
            io::u32 hash{};
            io::u32 name_offset{};
            io::u32 name_size{};
            io::u32 value{};
            io::u8 used{};
        };

        struct TextureEntry {
            io::u32 name_offset{};
            io::u32 name_size{};
            io::u32 x{};
            io::u32 y{};
            io::u32 width{};
            io::u32 height{};
        };

        struct BlockTextureNames {
            io::char_view front{};
            io::char_view back{};
            io::char_view left{};
            io::char_view right{};
            io::char_view top{};
            io::char_view bottom{};
        };

        struct BlockEntry {
            io::u32 name_offset{};
            io::u32 name_size{};
            io::u32 texture_ids[FACE_COUNT]{ INVALID_ID, INVALID_ID, INVALID_ID, INVALID_ID, INVALID_ID, INVALID_ID };
        };

        struct BlockDesc {
            io::char_view name{};
            BlockTextureNames textures{};
        };

        struct TextureAtlasOptions {
            io::u8 desired_channels = 4u;
            io::u32 atlas_padding_px = 1u;
            io::u32 max_blocks = 512u;
            io::u32 reserve_block_name_bytes = 16u * 1024u;
        };

        struct TextureAtlas {
            io::unique_bytes storage{};

            TextureEntry* textures{};
            BlockEntry* blocks{};
            NameIdEntry* texture_name_map{};
            NameIdEntry* block_name_map{};

            io::u8* atlas_pixels{};
            io::u8* decode_pixels{};
            io::u8* scratch{};
            char* names{};

            io::u32 atlas_width{};
            io::u32 atlas_height{};
            io::u8 atlas_channels{};

            io::u32 texture_count{};
            io::u32 block_count{};
            io::u32 max_blocks{};

            io::u32 texture_name_map_cap{};
            io::u32 block_name_map_cap{};

            io::u32 names_capacity{};
            io::u32 names_used{};

            io::usize decode_pixels_size{};
            io::usize scratch_size{};

            inline void reset() noexcept {
                storage.reset(nullptr);
                textures = nullptr;
                blocks = nullptr;
                texture_name_map = nullptr;
                block_name_map = nullptr;
                atlas_pixels = nullptr;
                decode_pixels = nullptr;
                scratch = nullptr;
                names = nullptr;
                atlas_width = 0;
                atlas_height = 0;
                atlas_channels = 0;
                texture_count = 0;
                block_count = 0;
                max_blocks = 0;
                texture_name_map_cap = 0;
                block_name_map_cap = 0;
                names_capacity = 0;
                names_used = 0;
                decode_pixels_size = 0;
                scratch_size = 0;
            }
        };

        static inline BlockTextureNames faces_all(io::char_view texture) noexcept {
            BlockTextureNames faces{};
            faces.front = texture;
            faces.back = texture;
            faces.left = texture;
            faces.right = texture;
            faces.top = texture;
            faces.bottom = texture;
            return faces;
        }

        static inline BlockTextureNames faces_top_bottom_side(io::char_view top,
                                                              io::char_view bottom,
                                                              io::char_view side) noexcept {
            BlockTextureNames faces{};
            faces.front = side;
            faces.back = side;
            faces.left = side;
            faces.right = side;
            faces.top = top;
            faces.bottom = bottom;
            return faces;
        }

        static inline BlockTextureNames faces_furnace(io::char_view front,
                                                      io::char_view side,
                                                      io::char_view top,
                                                      io::char_view bottom) noexcept {
            BlockTextureNames faces{};
            faces.front = front;
            faces.back = side;
            faces.left = side;
            faces.right = side;
            faces.top = top;
            faces.bottom = bottom;
            return faces;
        }

        static inline bool shader_from(io::char_view frag_filename,
                                       io::char_view vert_filename,
                                       gl::Shader& shader) noexcept;

        static inline bool texture_atlas_from(TextureAtlas& out_atlas,
                                              const TextureAtlasOptions& options = TextureAtlasOptions{}) noexcept;

        static inline bool register_blocks(TextureAtlas& atlas,
                                           io::view<const BlockDesc> blocks) noexcept;

        static inline io::u32 texture_id_of(const TextureAtlas& atlas,
                                            io::char_view texture_name) noexcept;

        static inline io::u32 block_id_of(const TextureAtlas& atlas,
                                          io::char_view block_name) noexcept;

        static inline io::char_view texture_name_of(const TextureAtlas& atlas,
                                                    io::u32 texture_id) noexcept;

        static inline io::char_view block_name_of(const TextureAtlas& atlas,
                                                  io::u32 block_id) noexcept;

        static inline bool texture_uv_of(const TextureAtlas& atlas, io::u32 texture_id,
                                         float& out_u0, float& out_v0,
                                         float& out_u1, float& out_v1) noexcept;

    private:
        struct TextureCandidate {
            io::string path{};
            io::string name{};
            stbi::ImagePlan plan{};
            io::u32 atlas_x{};
            io::u32 atlas_y{};
        };

        static inline bool read_and_compile_shader_from_roots(io::char_view filename,
                                                               io::u32& shader_id,
                                                               gl::ShaderType shader_type,
                                                               io::string& source) noexcept {
            shader_id = 0;
            const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
            for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
                io::StackOut<980> stack_path{};
                stack_path << roots[i] << PATH_SHADERS << filename;

                fs::File f{ stack_path.view(), io::OpenMode::Read };
                if (!f.is_open()) continue;

                source.clear();
                if (!f.read_all(source)) continue;
                if (source.empty()) continue;
                if (gl::Shader::compile_shader(shader_id, shader_type, source.c_str())) return true;
            }
            return false;
        }

        static inline io::u32 fnv1a_32(io::char_view value) noexcept {
            io::u32 hash = 2166136261u;
            for (io::usize i = 0; i < value.size(); ++i) {
                hash ^= static_cast<io::u8>(value[i]);
                hash *= 16777619u;
            }
            return hash;
        }

        static inline char ascii_lower(char c) noexcept {
            if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
            return c;
        }

        static inline bool iequals_ascii(io::char_view a, io::char_view b) noexcept {
            if (a.size() != b.size()) return false;
            for (io::usize i = 0; i < a.size(); ++i)
                if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
            return true;
        }

        static inline bool ends_with_ascii_i(io::char_view value, io::char_view suffix) noexcept {
            if (value.size() < suffix.size()) return false;
            const io::usize offset = value.size() - suffix.size();
            return iequals_ascii(value.slice(offset, suffix.size()), suffix);
        }

        static inline bool is_supported_image_path(io::char_view path) noexcept {
            return ends_with_ascii_i(path, ".png");
        }

        static inline bool checked_add(io::usize a, io::usize b, io::usize& out) noexcept {
            if (a > static_cast<io::usize>(-1) - b) return false;
            out = a + b;
            return true;
        }

        static inline bool checked_mul(io::usize a, io::usize b, io::usize& out) noexcept {
            if (a == 0 || b == 0) {
                out = 0;
                return true;
            }
            if (a > static_cast<io::usize>(-1) / b) return false;
            out = a * b;
            return true;
        }

        static inline io::usize align_up(io::usize value, io::usize alignment) noexcept {
            if (alignment == 0) return value;
            const io::usize mask = alignment - 1;
            return (value + mask) & ~mask;
        }

        static inline bool arena_push(io::usize& cursor, io::usize size, io::usize alignment, io::usize& out_offset) noexcept {
            const io::usize aligned = align_up(cursor, alignment);
            if (aligned < cursor) return false;
            io::usize end = 0;
            if (!checked_add(aligned, size, end)) return false;
            out_offset = aligned;
            cursor = end;
            return true;
        }

        static inline io::u32 next_pow2(io::u32 v) noexcept {
            if (v <= 1u) return 1u;
            --v;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            return v + 1u;
        }

        static inline io::u32 map_capacity_for(io::u32 count) noexcept {
            const io::u32 wanted = (count == 0u) ? 4u : (count * 2u + 1u);
            io::u32 cap = next_pow2(wanted);
            if (cap < 4u) cap = 4u;
            return cap;
        }

        static inline bool resolve_resources_root(io::string& out_root) noexcept {
            const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
            for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
                if (!fs::is_directory(roots[i])) continue;
                out_root.clear();
                return out_root.append(roots[i]);
            }
            out_root.clear();
            return out_root.append(PATH_RESOURCES);
        }

        static inline bool extract_path_stem(io::char_view path, io::char_view& out_stem) noexcept {
            if (path.empty()) return false;

            io::usize begin = 0;
            for (io::usize i = 0; i < path.size(); ++i)
                if (path[i] == '/' || path[i] == '\\') begin = i + 1;

            io::usize end = path.size();
            for (io::usize i = path.size(); i > begin; --i) {
                if (path[i - 1] == '.') {
                    end = i - 1;
                    break;
                }
            }

            if (end <= begin) return false;
            out_stem = path.slice(begin, end - begin);
            return true;
        }

        static inline bool collect_texture_files(io::char_view root_dir,
                                                 io::vector<io::string>& out_files) noexcept {
            out_files.clear();
            io::vector<io::string> dirs{};
            io::string root{};
            if (!root.append(root_dir)) return false;
            if (!dirs.push_back(io::move(root))) return false;

            for (io::usize i = 0; i < dirs.size(); ++i) {
                fs::directory_iterator it{ dirs[i].as_view() };
                while (!it.is_end()) {
                    const fs::directory_entry entry = *it;
                    if (entry.type == fs::file_type::directory) {
                        io::string sub{};
                        if (!sub.append(entry.path)) return false;
                        if (!dirs.push_back(io::move(sub))) return false;
                        ++it;
                        continue;
                    }

                    if (entry.type == fs::file_type::regular && is_supported_image_path(entry.path)) {
                        io::string file_path{};
                        if (!file_path.append(entry.path)) return false;
                        if (!out_files.push_back(io::move(file_path))) return false;
                    }
                    ++it;
                }
            }
            return true;
        }

        static inline bool map_clear(NameIdEntry* map, io::u32 cap) noexcept {
            if (!map || cap == 0u) return false;
            for (io::u32 i = 0; i < cap; ++i) map[i] = {};
            return true;
        }

        static inline bool map_key_equals(const TextureAtlas& atlas,
                                          const NameIdEntry& entry,
                                          io::char_view key) noexcept {
            if (entry.name_size != key.size()) return false;
            if (!atlas.names) return false;
            const char* p = atlas.names + entry.name_offset;
            for (io::usize i = 0; i < key.size(); ++i)
                if (p[i] != key[i]) return false;
            return true;
        }

        static inline bool map_insert(TextureAtlas& atlas,
                                      NameIdEntry* map,
                                      io::u32 cap,
                                      io::char_view key,
                                      io::u32 name_offset,
                                      io::u32 value) noexcept {
            if (!map || cap == 0u || key.empty()) return false;
            const io::u32 hash = fnv1a_32(key);
            const io::u32 mask = cap - 1u;
            io::u32 idx = hash & mask;

            for (io::u32 probe = 0; probe < cap; ++probe) {
                NameIdEntry& e = map[idx];
                if (!e.used) {
                    e.used = 1u;
                    e.hash = hash;
                    e.name_offset = name_offset;
                    e.name_size = static_cast<io::u32>(key.size());
                    e.value = value;
                    return true;
                }
                if (e.hash == hash && map_key_equals(atlas, e, key)) return false;
                idx = (idx + 1u) & mask;
            }
            return false;
        }

        static inline io::u32 map_find(const TextureAtlas& atlas,
                                       const NameIdEntry* map,
                                       io::u32 cap,
                                       io::char_view key) noexcept {
            if (!map || cap == 0u || key.empty()) return INVALID_ID;
            const io::u32 hash = fnv1a_32(key);
            const io::u32 mask = cap - 1u;
            io::u32 idx = hash & mask;

            for (io::u32 probe = 0; probe < cap; ++probe) {
                const NameIdEntry& e = map[idx];
                if (!e.used) return INVALID_ID;
                if (e.hash == hash && map_key_equals(atlas, e, key)) return e.value;
                idx = (idx + 1u) & mask;
            }
            return INVALID_ID;
        }

        static inline bool copy_name(TextureAtlas& atlas,
                                     io::char_view name,
                                     io::u32& out_offset,
                                     io::u32& out_size) noexcept {
            if (!atlas.names || name.empty()) return false;

            const io::u32 needed = static_cast<io::u32>(name.size() + 1u);
            if (atlas.names_used > atlas.names_capacity) return false;
            if (needed > atlas.names_capacity - atlas.names_used) return false;

            out_offset = atlas.names_used;
            out_size = static_cast<io::u32>(name.size());
            for (io::usize i = 0; i < name.size(); ++i)
                atlas.names[out_offset + i] = name[i];
            atlas.names[out_offset + name.size()] = '\0';
            atlas.names_used += needed;
            return true;
        }

        static inline bool try_pack_shelves(io::vector<TextureCandidate>& textures,
                                            io::u32 side,
                                            io::u32 padding_px,
                                            io::u32& out_used_height) noexcept {
            io::u32 x = 0u;
            io::u32 y = 0u;
            io::u32 row_h = 0u;
            io::u32 used_h = 0u;

            for (io::usize i = 0; i < textures.size(); ++i) {
                TextureCandidate& t = textures[i];
                const io::u32 w = t.plan.width;
                const io::u32 h = t.plan.height;
                if (w == 0u || h == 0u || w > side || h > side) return false;

                if (x > 0u && x + w > side) {
                    x = 0u;
                    y += row_h;
                    row_h = 0u;
                }
                if (y + h > side) return false;

                t.atlas_x = x;
                t.atlas_y = y;

                if (y + h > used_h) used_h = y + h;

                const io::u32 adv_w = w + padding_px;
                const io::u32 adv_h = h + padding_px;
                x += adv_w;
                if (adv_h > row_h) row_h = adv_h;
            }

            out_used_height = (used_h == 0u) ? 1u : used_h;
            return true;
        }

        static inline bool read_file_bytes(io::char_view path, io::string& out_bytes) noexcept {
            fs::File f{ path, io::OpenMode::Read | io::OpenMode::Binary };
            if (!f.is_open()) return false;
            return f.read_all(out_bytes);
        }

        static inline bool texture_name_exists(const io::vector<TextureCandidate>& textures,
                                               io::char_view name) noexcept {
            for (io::usize i = 0; i < textures.size(); ++i)
                if (textures[i].name.as_view() == name) return true;
            return false;
        }
    }; // struct ResourceManager

    bool ResourceManager::shader_from(io::char_view frag_filename,
                                      io::char_view vert_filename,
                                      gl::Shader& shader) noexcept {
        io::u32 frag_id{};
        io::u32 vert_id{};
        io::u32 prog_id{};

        io::string source{};
        if (!read_and_compile_shader_from_roots(vert_filename, vert_id, gl::ShaderType::VertexShader, source)) {
            log(LogFrom::Resource, LogWhat::CompileShader);
            return false;
        }
        if (!read_and_compile_shader_from_roots(frag_filename, frag_id, gl::ShaderType::FragmentShader, source)) {
            if (vert_id) gl::DeleteShader(vert_id);
            log(LogFrom::Resource, LogWhat::CompileShader);
            return false;
        }

        const bool ok_prog = gl::Shader::link_program(prog_id, vert_id, frag_id);
        gl::DeleteShader(vert_id);
        gl::DeleteShader(frag_id);
        if (!ok_prog) {
            log(LogFrom::Resource, LogWhat::LinkShaderProgram);
            return false;
        }
        shader.~Shader();
        new (&shader) gl::Shader{ prog_id };
        return true;
    } // shader_from

    bool ResourceManager::texture_atlas_from(TextureAtlas& out_atlas,
                                             const TextureAtlasOptions& options) noexcept {
        out_atlas.reset();

        io::string root{};
        if (!resolve_resources_root(root)) return false;

        io::string textures_root{};
        if (!fs::path_join(root, PATH_TEXTURES, textures_root)) return false;
        if (!fs::is_directory(textures_root)) {
#ifdef _DEBUG
            io::out << "[Resource] textures directory not found: " << textures_root << '\n';
#endif
            return false;
        }

        io::vector<io::string> texture_files{};
        if (!collect_texture_files(textures_root.as_view(), texture_files)) return false;
        if (texture_files.empty()) {
#ifdef _DEBUG
            io::out << "[Resource] no image files found in: " << textures_root << '\n';
#endif
            return false;
        }

        io::vector<TextureCandidate> textures{};
        stbi::BatchPlanner planner{};
        io::usize texture_name_bytes = 0;
        io::u32 max_w = 1u;
        io::u32 max_h = 1u;

        stbi::DecodeOptions decode_opt{};
        decode_opt.desired_channels = options.desired_channels == 0u ? 4u :
                                      (options.desired_channels > 4u ? 4u : options.desired_channels);
        decode_opt.sample_type = stbi::SampleType::U8;
        decode_opt.flip_vertically = true;

        io::string file_bytes{};
        for (io::usize i = 0; i < texture_files.size(); ++i) {
            if (!read_file_bytes(texture_files[i].as_view(), file_bytes)) {
#ifdef _DEBUG
                io::out << "[Resource] failed to read texture: " << texture_files[i] << '\n';
#endif
                return false;
            }

            stbi::Decoder decoder{};
            if (!decoder.ReadBytes(reinterpret_cast<const io::u8*>(file_bytes.data()), file_bytes.size())) return false;

            stbi::ImagePlan plan{};
            if (!decoder.Plan(decode_opt, plan)) {
#ifdef _DEBUG
                io::out << "[Resource] stb::Plan failed for: " << texture_files[i]
                        << " reason: " << decoder.FailureReason() << '\n';
#endif
                return false;
            }
            if (!planner.Add(plan)) return false;

            io::char_view stem{};
            if (!extract_path_stem(texture_files[i].as_view(), stem)) {
#ifdef _DEBUG
                io::out << "[Resource] texture file has invalid name: " << texture_files[i] << '\n';
#endif
                return false;
            }
            if (texture_name_exists(textures, stem)) {
#ifdef _DEBUG
                io::out << "[Resource] duplicated texture name: " << stem
                        << " (file: " << texture_files[i] << ")\n";
#endif
                return false;
            }

            TextureCandidate candidate{};
            if (!candidate.path.append(texture_files[i].as_view())) return false;
            if (!candidate.name.append(stem)) return false;
            candidate.plan = plan;
            if (!textures.push_back(io::move(candidate))) return false;

            texture_name_bytes += stem.size() + 1u;
            if (plan.width > max_w) max_w = plan.width;
            if (plan.height > max_h) max_h = plan.height;
        }

        io::u32 atlas_side = next_pow2(max_w > max_h ? max_w : max_h);
        if (atlas_side < 4u) atlas_side = 4u;

        io::u32 atlas_used_h = 0u;
        while (!try_pack_shelves(textures, atlas_side, options.atlas_padding_px, atlas_used_h)) {
            if (atlas_side >= 16384u) return false;
            atlas_side <<= 1;
        }

        const io::u32 texture_count = static_cast<io::u32>(textures.size());
        const io::u32 block_cap = options.max_blocks == 0u ? 1u : options.max_blocks;
        const io::u32 tex_map_cap = map_capacity_for(texture_count);
        const io::u32 block_map_cap = map_capacity_for(block_cap);
        const io::u32 names_cap = static_cast<io::u32>(texture_name_bytes + options.reserve_block_name_bytes);
        const io::u8 channels = decode_opt.desired_channels;

        io::usize atlas_pixels_bytes = 0;
        {
            io::usize pixel_count = 0;
            if (!checked_mul(static_cast<io::usize>(atlas_side), static_cast<io::usize>(atlas_side), pixel_count)) return false;
            if (!checked_mul(pixel_count, static_cast<io::usize>(channels), atlas_pixels_bytes)) return false;
        }

        const io::usize max_decode_pixels = planner.Get().max_pixel_bytes;
        const io::usize max_decode_scratch = planner.ReusableScratchBytes();

        io::usize total_bytes = 0;
        io::usize off_textures = 0;
        io::usize off_blocks = 0;
        io::usize off_tex_map = 0;
        io::usize off_block_map = 0;
        io::usize off_names = 0;
        io::usize off_atlas_pixels = 0;
        io::usize off_decode_pixels = 0;
        io::usize off_scratch = 0;

        if (!arena_push(total_bytes, sizeof(TextureEntry) * texture_count, alignof(TextureEntry), off_textures)) return false;
        if (!arena_push(total_bytes, sizeof(BlockEntry) * block_cap, alignof(BlockEntry), off_blocks)) return false;
        if (!arena_push(total_bytes, sizeof(NameIdEntry) * tex_map_cap, alignof(NameIdEntry), off_tex_map)) return false;
        if (!arena_push(total_bytes, sizeof(NameIdEntry) * block_map_cap, alignof(NameIdEntry), off_block_map)) return false;
        if (!arena_push(total_bytes, static_cast<io::usize>(names_cap), alignof(char), off_names)) return false;
        if (!arena_push(total_bytes, atlas_pixels_bytes, alignof(io::u8), off_atlas_pixels)) return false;
        if (!arena_push(total_bytes, max_decode_pixels, alignof(io::u8), off_decode_pixels)) return false;
        if (!arena_push(total_bytes, max_decode_scratch, alignof(io::u8), off_scratch)) return false;

        io::u8* mem = static_cast<io::u8*>(io::alloc_aligned(total_bytes ? total_bytes : 1u, alignof(io::u64)));
        if (!mem) return false;

        out_atlas.storage.reset(mem);
        out_atlas.textures = reinterpret_cast<TextureEntry*>(mem + off_textures);
        out_atlas.blocks = reinterpret_cast<BlockEntry*>(mem + off_blocks);
        out_atlas.texture_name_map = reinterpret_cast<NameIdEntry*>(mem + off_tex_map);
        out_atlas.block_name_map = reinterpret_cast<NameIdEntry*>(mem + off_block_map);
        out_atlas.names = reinterpret_cast<char*>(mem + off_names);
        out_atlas.atlas_pixels = mem + off_atlas_pixels;
        out_atlas.decode_pixels = mem + off_decode_pixels;
        out_atlas.scratch = mem + off_scratch;

        out_atlas.atlas_width = atlas_side;
        out_atlas.atlas_height = atlas_side;
        out_atlas.atlas_channels = channels;
        out_atlas.texture_count = texture_count;
        out_atlas.block_count = 0u;
        out_atlas.max_blocks = block_cap;
        out_atlas.texture_name_map_cap = tex_map_cap;
        out_atlas.block_name_map_cap = block_map_cap;
        out_atlas.names_capacity = names_cap;
        out_atlas.names_used = 0u;
        out_atlas.decode_pixels_size = max_decode_pixels;
        out_atlas.scratch_size = max_decode_scratch;
        if (out_atlas.scratch_size == 0u) out_atlas.scratch = nullptr;

        if (!map_clear(out_atlas.texture_name_map, out_atlas.texture_name_map_cap)) {
            out_atlas.reset();
            return false;
        }
        if (!map_clear(out_atlas.block_name_map, out_atlas.block_name_map_cap)) {
            out_atlas.reset();
            return false;
        }
        for (io::u32 i = 0; i < out_atlas.max_blocks; ++i) out_atlas.blocks[i] = {};
        for (io::usize i = 0; i < atlas_pixels_bytes; ++i) out_atlas.atlas_pixels[i] = 0u;

        for (io::u32 i = 0; i < texture_count; ++i) {
            io::u32 name_offset = 0u;
            io::u32 name_size = 0u;
            if (!copy_name(out_atlas, textures[i].name.as_view(), name_offset, name_size)) {
                out_atlas.reset();
                return false;
            }

            TextureEntry& t = out_atlas.textures[i];
            t.name_offset = name_offset;
            t.name_size = name_size;
            t.x = textures[i].atlas_x;
            t.y = textures[i].atlas_y;
            t.width = textures[i].plan.width;
            t.height = textures[i].plan.height;

            if (!map_insert(out_atlas, out_atlas.texture_name_map, out_atlas.texture_name_map_cap,
                            textures[i].name.as_view(), name_offset, i)) {
                out_atlas.reset();
                return false;
            }
        }

        for (io::u32 i = 0; i < texture_count; ++i) {
            TextureCandidate& tc = textures[i];

            if (!read_file_bytes(tc.path.as_view(), file_bytes)) {
                out_atlas.reset();
                return false;
            }

            stbi::Decoder decoder{};
            if (!decoder.ReadBytes(reinterpret_cast<const io::u8*>(file_bytes.data()), file_bytes.size())) {
                out_atlas.reset();
                return false;
            }
            if (!decoder.Decode(tc.plan,
                                out_atlas.scratch, out_atlas.scratch_size,
                                out_atlas.decode_pixels, out_atlas.decode_pixels_size)) {
#ifdef _DEBUG
                io::out << "[Resource] stb::Decode failed for: " << tc.path
                        << " reason: " << decoder.FailureReason() << '\n';
#endif
                out_atlas.reset();
                return false;
            }

            const io::usize row_bytes = static_cast<io::usize>(tc.plan.width) * static_cast<io::usize>(channels);
            for (io::u32 row = 0; row < tc.plan.height; ++row) {
                io::u8* dst = out_atlas.atlas_pixels
                            + (static_cast<io::usize>(tc.atlas_y + row) * static_cast<io::usize>(out_atlas.atlas_width)
                            + static_cast<io::usize>(tc.atlas_x)) * static_cast<io::usize>(channels);
                const io::u8* src = out_atlas.decode_pixels + row_bytes * static_cast<io::usize>(row);
                for (io::usize k = 0; k < row_bytes; ++k) dst[k] = src[k];
            }
        }

        return true;
    }

    bool ResourceManager::register_blocks(TextureAtlas& atlas,
                                          io::view<const BlockDesc> blocks) noexcept {
        if (!atlas.blocks || !atlas.block_name_map || !atlas.names) return false;
        if (blocks.size() > atlas.max_blocks) return false;

        atlas.block_count = 0u;
        if (!map_clear(atlas.block_name_map, atlas.block_name_map_cap)) return false;

        for (io::usize i = 0; i < blocks.size(); ++i) {
            const BlockDesc& in = blocks[i];
            if (in.name.empty()) return false;

            const io::u32 front = texture_id_of(atlas, in.textures.front);
            const io::u32 back = texture_id_of(atlas, in.textures.back);
            const io::u32 left = texture_id_of(atlas, in.textures.left);
            const io::u32 right = texture_id_of(atlas, in.textures.right);
            const io::u32 top = texture_id_of(atlas, in.textures.top);
            const io::u32 bottom = texture_id_of(atlas, in.textures.bottom);
            if (front == INVALID_ID || back == INVALID_ID || left == INVALID_ID ||
                right == INVALID_ID || top == INVALID_ID || bottom == INVALID_ID) {
#ifdef _DEBUG
                io::out << "[Resource] block '" << in.name
                        << "' refers to unknown texture name\n";
#endif
                return false;
            }

            io::u32 name_offset = 0u;
            io::u32 name_size = 0u;
            if (!copy_name(atlas, in.name, name_offset, name_size)) return false;

            BlockEntry& out = atlas.blocks[atlas.block_count];
            out.name_offset = name_offset;
            out.name_size = name_size;
            out.texture_ids[0] = front;
            out.texture_ids[1] = back;
            out.texture_ids[2] = left;
            out.texture_ids[3] = right;
            out.texture_ids[4] = top;
            out.texture_ids[5] = bottom;

            if (!map_insert(atlas, atlas.block_name_map, atlas.block_name_map_cap,
                            in.name, name_offset, atlas.block_count)) return false;
            ++atlas.block_count;
        }
        return true;
    }

    io::u32 ResourceManager::texture_id_of(const TextureAtlas& atlas,
                                           io::char_view texture_name) noexcept {
        return map_find(atlas, atlas.texture_name_map, atlas.texture_name_map_cap, texture_name);
    }

    io::u32 ResourceManager::block_id_of(const TextureAtlas& atlas,
                                         io::char_view block_name) noexcept {
        return map_find(atlas, atlas.block_name_map, atlas.block_name_map_cap, block_name);
    }

    io::char_view ResourceManager::texture_name_of(const TextureAtlas& atlas,
                                                   io::u32 texture_id) noexcept {
        if (!atlas.textures || !atlas.names || texture_id >= atlas.texture_count) return {};
        const TextureEntry& t = atlas.textures[texture_id];
        return io::char_view{ atlas.names + t.name_offset, t.name_size };
    }

    io::char_view ResourceManager::block_name_of(const TextureAtlas& atlas,
                                                 io::u32 block_id) noexcept {
        if (!atlas.blocks || !atlas.names || block_id >= atlas.block_count) return {};
        const BlockEntry& b = atlas.blocks[block_id];
        return io::char_view{ atlas.names + b.name_offset, b.name_size };
    }

    bool ResourceManager::texture_uv_of(const TextureAtlas& atlas, io::u32 texture_id,
                                        float& out_u0, float& out_v0,
                                        float& out_u1, float& out_v1) noexcept {
        if (!atlas.textures || atlas.atlas_width == 0u || atlas.atlas_height == 0u) return false;
        if (texture_id >= atlas.texture_count) return false;

        const TextureEntry& t = atlas.textures[texture_id];
        const float inv_w = 1.0f / static_cast<float>(atlas.atlas_width);
        const float inv_h = 1.0f / static_cast<float>(atlas.atlas_height);

        out_u0 = static_cast<float>(t.x) * inv_w;
        out_v0 = static_cast<float>(t.y) * inv_h;
        out_u1 = static_cast<float>(t.x + t.width) * inv_w;
        out_v1 = static_cast<float>(t.y + t.height) * inv_h;
        return true;
    }
} // namespace ge
