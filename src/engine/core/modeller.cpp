#include "modeller.hpp"

#include "config.hpp"

namespace {
    struct Span {
        io::usize begin{};
        io::usize end{};
    };

    struct BufferViewRec {
        io::u32 byte_offset{};
        io::u32 byte_length{};
        io::u32 byte_stride{};
    };

    enum class AccessorType : io::u8 { Unknown = 0, Scalar, Vec2, Vec3, Vec4, Mat4 };

    struct AccessorRec {
        io::u32 buffer_view{};
        io::u32 byte_offset{};
        io::u32 component_type{};
        io::u32 count{};
        AccessorType type{ AccessorType::Unknown };
    };

    struct PrimitiveRec {
        io::u32 mesh_index{};
        io::u32 pos_accessor{};
        io::u32 normal_accessor{};
        io::u32 uv_accessor{};
        io::u32 joints_accessor{};
        io::u32 weights_accessor{};
        io::u32 indices_accessor{};
        io::u32 material{};
    };

    struct TextureRec {
        io::u32 source{};
        io::string name{};
    };

    struct MaterialRec {
        io::u32 texture_index{};
    };

    struct NodeRec {
        io::string name{};
        float tx = 0.f;
        float ty = 0.f;
        float tz = 0.f;
        float rx = 0.f;
        float ry = 0.f;
        float rz = 0.f;
        float rw = 1.f;
        float sx = 1.f;
        float sy = 1.f;
        float sz = 1.f;
        io::i32 mesh_index = -1;
        io::vector<io::u32> children{};
    };

    static bool checked_add(io::usize a, io::usize b, io::usize& out) noexcept {
        if (a > static_cast<io::usize>(-1) - b) return false;
        out = a + b;
        return true;
    }

    static char ascii_lower(char c) noexcept {
        if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
        return c;
    }

    static bool ieq_ascii(io::char_view a, io::char_view b) noexcept {
        if (a.size() != b.size()) return false;
        for (io::usize i = 0; i < a.size(); ++i)
            if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
        return true;
    }

    static bool ends_with_ascii_i(io::char_view value, io::char_view suffix) noexcept {
        if (value.size() < suffix.size()) return false;
        const io::usize off = value.size() - suffix.size();
        return ieq_ascii(value.slice(off, suffix.size()), suffix);
    }

    static bool extract_path_stem(io::char_view path, io::char_view& out_stem) noexcept {
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

    static io::char_view strip_ext(io::char_view name) noexcept {
        for (io::usize i = name.size(); i > 0; --i)
            if (name[i - 1] == '.') return name.slice(0, i - 1);
        return name;
    }

    static bool collect_model_files(io::char_view root_dir, io::vector<io::string>& out_files) noexcept {
        out_files.clear();
        if (!fs::is_directory(root_dir)) return false;

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

                if (entry.type == fs::file_type::regular && ends_with_ascii_i(entry.path, ".gltf")) {
                    io::string path{};
                    if (!path.append(entry.path)) return false;
                    if (!out_files.push_back(io::move(path))) return false;
                }
                ++it;
            }
        }
        return true;
    }

    static bool read_file_bytes(io::char_view path, io::string& out) noexcept {
        fs::File f{ path, io::OpenMode::Read | io::OpenMode::Binary };
        if (!f.is_open()) return false;
        return f.read_all(out);
    }

    static bool parse_u32(io::char_view s, io::usize pos, io::u32& out, io::usize& next) noexcept {
        if (pos >= s.size()) return false;
        io::u64 v = 0;
        io::usize i = pos;
        bool any = false;
        while (i < s.size()) {
            const char c = s[i];
            if (c < '0' || c > '9') break;
            any = true;
            v = v * 10u + static_cast<io::u64>(c - '0');
            if (v > 0xFFFFFFFFull) return false;
            ++i;
        }
        if (!any) return false;
        out = static_cast<io::u32>(v);
        next = i;
        return true;
    }

    static bool parse_float(io::char_view s, io::usize pos, float& out, io::usize& next) noexcept {
        if (pos >= s.size()) return false;
        io::usize i = pos;
        int sign = 1;
        if (s[i] == '-') {
            sign = -1;
            ++i;
        } else if (s[i] == '+') {
            ++i;
        }

        double value = 0.0;
        bool any = false;
        while (i < s.size()) {
            const char c = s[i];
            if (c < '0' || c > '9') break;
            any = true;
            value = value * 10.0 + static_cast<double>(c - '0');
            ++i;
        }

        if (i < s.size() && s[i] == '.') {
            ++i;
            double place = 0.1;
            while (i < s.size()) {
                const char c = s[i];
                if (c < '0' || c > '9') break;
                any = true;
                value += static_cast<double>(c - '0') * place;
                place *= 0.1;
                ++i;
            }
        }
        if (!any) return false;

        out = static_cast<float>(value * static_cast<double>(sign));
        next = i;
        return true;
    }

    static bool parse_float3(io::char_view s, Span range, io::char_view key, float& x, float& y, float& z) noexcept {
        const io::usize p = s.find(key, range.begin);
        if (p == io::npos || p >= range.end) return false;
        io::usize at = p + key.size();
        if (at >= s.size() || s[at] != '[') return false;
        ++at;
        if (!parse_float(s, at, x, at)) return false;
        if (at >= s.size() || s[at] != ',') return false;
        ++at;
        if (!parse_float(s, at, y, at)) return false;
        if (at >= s.size() || s[at] != ',') return false;
        ++at;
        if (!parse_float(s, at, z, at)) return false;
        return true;
    }

    static bool parse_float4(io::char_view s, Span range, io::char_view key, float& x, float& y, float& z, float& w) noexcept {
        const io::usize p = s.find(key, range.begin);
        if (p == io::npos || p >= range.end) return false;
        io::usize at = p + key.size();
        if (at >= s.size() || s[at] != '[') return false;
        ++at;
        if (!parse_float(s, at, x, at)) return false;
        if (at >= s.size() || s[at] != ',') return false;
        ++at;
        if (!parse_float(s, at, y, at)) return false;
        if (at >= s.size() || s[at] != ',') return false;
        ++at;
        if (!parse_float(s, at, z, at)) return false;
        if (at >= s.size() || s[at] != ',') return false;
        ++at;
        if (!parse_float(s, at, w, at)) return false;
        return true;
    }

    static bool parse_int_field(io::char_view s, Span range, io::char_view key, io::u32& out) noexcept {
        const io::usize p = s.find(key, range.begin);
        if (p == io::npos || p >= range.end) return false;
        io::usize at = p + key.size();
        return parse_u32(s, at, out, at);
    }

    static bool parse_string_field(io::char_view s, Span range, io::char_view key, io::char_view& out) noexcept {
        const io::usize p = s.find(key, range.begin);
        if (p == io::npos || p >= range.end) return false;
        io::usize at = p + key.size();
        const io::usize end = s.find('"', at);
        if (end == io::npos || end > range.end) return false;
        out = s.slice(at, end - at);
        return true;
    }

    static bool find_bracket_range(io::char_view s, io::char_view key, char open_ch, char close_ch, Span& out) noexcept {
        const io::usize key_pos = s.find(key);
        if (key_pos == io::npos) return false;
        const io::usize open_pos = s.find(open_ch, key_pos + key.size());
        if (open_pos == io::npos) return false;

        io::u32 depth = 0;
        for (io::usize i = open_pos; i < s.size(); ++i) {
            if (s[i] == open_ch) ++depth;
            else if (s[i] == close_ch) {
                if (depth == 0) return false;
                --depth;
                if (depth == 0) {
                    out.begin = open_pos;
                    out.end = i + 1;
                    return true;
                }
            }
        }
        return false;
    }

    static bool find_object_array_range(io::char_view s, io::char_view key, Span& out) noexcept {
        io::usize search_from = 0u;
        while (search_from < s.size()) {
            const io::usize key_pos = s.find(key, search_from);
            if (key_pos == io::npos) return false;
            const io::usize open_pos = s.find('[', key_pos + key.size());
            if (open_pos == io::npos) return false;

            io::u32 depth = 0u;
            io::usize close_pos = io::npos;
            for (io::usize i = open_pos; i < s.size(); ++i) {
                if (s[i] == '[') ++depth;
                else if (s[i] == ']') {
                    if (depth == 0u) return false;
                    --depth;
                    if (depth == 0u) {
                        close_pos = i;
                        break;
                    }
                }
            }
            if (close_pos == io::npos) return false;

            const io::usize object_pos = s.find('{', open_pos + 1u);
            if (object_pos != io::npos && object_pos < close_pos) {
                out.begin = open_pos;
                out.end = close_pos + 1u;
                return true;
            }
            search_from = key_pos + key.size();
        }
        return false;
    }

    static bool collect_object_ranges(io::char_view s, Span array_range, io::vector<Span>& out_ranges) noexcept {
        out_ranges.clear();
        if (array_range.end <= array_range.begin + 1) return true;
        io::usize i = array_range.begin + 1;
        while (i + 1 < array_range.end) {
            const io::usize ob = s.find('{', i);
            if (ob == io::npos || ob >= array_range.end) break;
            io::u32 depth = 0;
            io::usize oe = ob;
            bool found = false;
            for (; oe < array_range.end; ++oe) {
                if (s[oe] == '{') ++depth;
                else if (s[oe] == '}') {
                    if (depth == 0) return false;
                    --depth;
                    if (depth == 0) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found || oe >= array_range.end) return false;
            if (!out_ranges.push_back(Span{ ob, oe + 1 })) return false;
            i = oe + 1;
        }
        return true;
    }

    static bool parse_u32_array(io::char_view s, Span range, io::char_view key, io::vector<io::u32>& out) noexcept {
        out.clear();
        const io::usize p = s.find(key, range.begin);
        if (p == io::npos || p >= range.end) return false;
        io::usize at = p + key.size();
        if (at >= s.size() || s[at] != '[') return false;
        ++at;
        while (at < s.size() && at < range.end) {
            while (at < s.size() && at < range.end && (s[at] == ' ' || s[at] == '\n' || s[at] == '\r' || s[at] == '\t' || s[at] == ',')) ++at;
            if (at >= s.size() || at >= range.end) break;
            if (s[at] == ']') break;
            io::u32 v = 0u;
            io::usize next = at;
            if (!parse_u32(s, at, v, next)) return false;
            if (!out.push_back(v)) return false;
            at = next;
        }
        return true;
    }

    static int b64_val(char c) noexcept {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    static bool base64_decode(io::char_view in, io::vector<io::u8>& out) noexcept {
        out.clear();
        io::u32 acc = 0;
        io::u32 bits = 0;
        for (io::usize i = 0; i < in.size(); ++i) {
            const char c = in[i];
            if (c == '=') break;
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ') continue;
            const int v = b64_val(c);
            if (v < 0) return false;
            acc = (acc << 6) | static_cast<io::u32>(v);
            bits += 6u;
            while (bits >= 8u) {
                bits -= 8u;
                const io::u8 b = static_cast<io::u8>((acc >> bits) & 0xFFu);
                if (!out.push_back(b)) return false;
            }
        }
        return true;
    }

    static bool parse_buffer_uri(io::char_view json, io::char_view& out_uri) noexcept {
        Span buffers{};
        if (!find_bracket_range(json, "\"buffers\":", '[', ']', buffers)) return false;
        io::vector<Span> objs{};
        if (!collect_object_ranges(json, buffers, objs)) return false;
        if (objs.empty()) return false;
        return parse_string_field(json, objs[0], "\"uri\":\"", out_uri);
    }

    static bool extract_parent_dir(io::char_view path, io::char_view& out_dir) noexcept {
        if (path.empty()) return false;
        for (io::usize i = path.size(); i > 0; --i) {
            const char c = path[i - 1];
            if (c == '/' || c == '\\') {
                out_dir = path.slice(0, i - 1);
                return true;
            }
        }
        out_dir = io::char_view{};
        return true;
    }

    static bool load_buffer_bytes(io::char_view json,
                                  io::char_view gltf_path,
                                  io::vector<io::u8>& out_bytes) noexcept {
        io::char_view uri{};
        if (!parse_buffer_uri(json, uri)) return false;
        if (uri.empty()) return false;

        if (uri.find("data:") == 0) {
            const io::usize comma = uri.find(',');
            if (comma == io::npos || comma + 1 >= uri.size()) return false;
            return base64_decode(uri.slice(comma + 1, uri.size() - comma - 1), out_bytes);
        }

        io::char_view gltf_dir{};
        if (!extract_parent_dir(gltf_path, gltf_dir)) return false;
        io::string bin_path{};
        if (gltf_dir.empty()) {
            if (!bin_path.append(uri)) return false;
        } else {
            if (!fs::path_join(gltf_dir, uri, bin_path)) return false;
        }

        io::string bytes{};
        if (!read_file_bytes(bin_path.as_view(), bytes)) return false;
        if (!out_bytes.resize(bytes.size())) return false;
        for (io::usize i = 0; i < bytes.size(); ++i)
            out_bytes[i] = static_cast<io::u8>(bytes[i]);
        return true;
    }

    static bool parse_image_names(io::char_view json, io::vector<io::string>& out_names) noexcept {
        out_names.clear();
        Span arr{};
        if (!find_bracket_range(json, "\"images\":", '[', ']', arr)) return true;
        io::vector<Span> objs{};
        if (!collect_object_ranges(json, arr, objs)) return false;

        for (io::usize i = 0; i < objs.size(); ++i) {
            io::char_view image_name{};
            if (!parse_string_field(json, objs[i], "\"name\":\"", image_name)) {
                if (!parse_string_field(json, objs[i], "\"uri\":\"", image_name)) image_name = {};
            }
            image_name = strip_ext(image_name);

            io::string item{};
            if (!item.append(image_name)) return false;
            if (!out_names.push_back(io::move(item))) return false;
        }
        return true;
    }

    static bool parse_textures(io::char_view json, io::vector<TextureRec>& out_textures) noexcept {
        out_textures.clear();
        Span arr{};
        if (!find_bracket_range(json, "\"textures\":", '[', ']', arr)) return false;
        io::vector<Span> objs{};
        if (!collect_object_ranges(json, arr, objs)) return false;

        for (io::usize i = 0; i < objs.size(); ++i) {
            TextureRec t{};
            if (!parse_int_field(json, objs[i], "\"source\":", t.source)) t.source = static_cast<io::u32>(-1);
            io::char_view name{};
            if (parse_string_field(json, objs[i], "\"name\":\"", name))
                if (!t.name.append(strip_ext(name))) return false;
            if (!out_textures.push_back(io::move(t))) return false;
        }
        return true;
    }

    static bool resolve_texture_name(const io::vector<TextureRec>& textures,
                                     const io::vector<io::string>& image_names,
                                     io::u32 texture_index,
                                     io::char_view& out_name) noexcept {
        out_name = {};
        if (texture_index >= textures.size()) return false;
        const TextureRec& t = textures[texture_index];

        if (t.source < image_names.size())
            out_name = image_names[t.source].as_view();
        if (out_name.empty() && !t.name.empty())
            out_name = t.name.as_view();
        return !out_name.empty();
    }

    static bool parse_buffer_views(io::char_view json, io::vector<BufferViewRec>& out) noexcept {
        out.clear();
        Span arr{};
        if (!find_bracket_range(json, "\"bufferViews\":", '[', ']', arr)) return false;
        io::vector<Span> objs{};
        if (!collect_object_ranges(json, arr, objs)) return false;

        for (io::usize i = 0; i < objs.size(); ++i) {
            BufferViewRec r{};
            if (!parse_int_field(json, objs[i], "\"byteOffset\":", r.byte_offset)) r.byte_offset = 0u;
            if (!parse_int_field(json, objs[i], "\"byteLength\":", r.byte_length)) return false;
            if (!parse_int_field(json, objs[i], "\"byteStride\":", r.byte_stride)) r.byte_stride = 0u;
            if (!out.push_back(r)) return false;
        }
        return true;
    }

    static AccessorType parse_accessor_type(io::char_view t) noexcept {
        if (t == "SCALAR") return AccessorType::Scalar;
        if (t == "VEC2") return AccessorType::Vec2;
        if (t == "VEC3") return AccessorType::Vec3;
        if (t == "VEC4") return AccessorType::Vec4;
        if (t == "MAT4") return AccessorType::Mat4;
        return AccessorType::Unknown;
    }

    static bool parse_accessors(io::char_view json, io::vector<AccessorRec>& out) noexcept {
        out.clear();
        Span arr{};
        if (!find_bracket_range(json, "\"accessors\":", '[', ']', arr)) return false;
        io::vector<Span> objs{};
        if (!collect_object_ranges(json, arr, objs)) return false;

        for (io::usize i = 0; i < objs.size(); ++i) {
            AccessorRec a{};
            if (!parse_int_field(json, objs[i], "\"bufferView\":", a.buffer_view)) return false;
            if (!parse_int_field(json, objs[i], "\"byteOffset\":", a.byte_offset)) a.byte_offset = 0u;
            if (!parse_int_field(json, objs[i], "\"componentType\":", a.component_type)) return false;
            if (!parse_int_field(json, objs[i], "\"count\":", a.count)) return false;
            io::char_view type{};
            if (!parse_string_field(json, objs[i], "\"type\":\"", type)) return false;
            a.type = parse_accessor_type(type);
            if (!out.push_back(a)) return false;
        }
        return true;
    }

    static bool parse_nodes(io::char_view json, io::vector<NodeRec>& out) noexcept {
        out.clear();
        Span arr{};
        if (!find_object_array_range(json, "\"nodes\":", arr)) return true;
        io::vector<Span> objs{};
        if (!collect_object_ranges(json, arr, objs)) return false;

        for (io::usize i = 0; i < objs.size(); ++i) {
            NodeRec n{};
            io::char_view name{};
            if (parse_string_field(json, objs[i], "\"name\":\"", name)) {
                if (!n.name.append(name)) return false;
            } else {
                io::StackOut<32> ss{};
                ss << "node_" << i;
                if (!n.name.append(ss.view())) return false;
            }

            (void)parse_float3(json, objs[i], "\"translation\":", n.tx, n.ty, n.tz);
            (void)parse_float4(json, objs[i], "\"rotation\":", n.rx, n.ry, n.rz, n.rw);
            (void)parse_float3(json, objs[i], "\"scale\":", n.sx, n.sy, n.sz);
            io::u32 mesh_index = 0u;
            if (parse_int_field(json, objs[i], "\"mesh\":", mesh_index))
                n.mesh_index = static_cast<io::i32>(mesh_index);
            (void)parse_u32_array(json, objs[i], "\"children\":", n.children);

            if (!out.push_back(io::move(n))) return false;
        }
        return true;
    }

    static bool parse_materials(io::char_view json, io::vector<MaterialRec>& out) noexcept {
        out.clear();
        Span arr{};
        if (!find_bracket_range(json, "\"materials\":", '[', ']', arr)) return false;
        io::vector<Span> objs{};
        if (!collect_object_ranges(json, arr, objs)) return false;

        for (io::usize i = 0; i < objs.size(); ++i) {
            MaterialRec m{};
            if (!parse_int_field(json, objs[i], "\"index\":", m.texture_index)) return false;
            if (!out.push_back(m)) return false;
        }
        return true;
    }

    static bool parse_primitives(io::char_view json, io::vector<PrimitiveRec>& out) noexcept {
        out.clear();
        Span meshes{};
        if (!find_bracket_range(json, "\"meshes\":", '[', ']', meshes)) return false;

        io::vector<Span> mesh_objs{};
        if (!collect_object_ranges(json, meshes, mesh_objs)) return false;
        if (mesh_objs.empty()) return false;

        for (io::u32 mesh_index = 0u; mesh_index < static_cast<io::u32>(mesh_objs.size()); ++mesh_index) {
            Span prim_arr{};
            io::char_view mesh_sub = json.slice(mesh_objs[mesh_index].begin, mesh_objs[mesh_index].end - mesh_objs[mesh_index].begin);
            if (!find_bracket_range(mesh_sub, "\"primitives\":", '[', ']', prim_arr))
                continue;
            prim_arr.begin += mesh_objs[mesh_index].begin;
            prim_arr.end += mesh_objs[mesh_index].begin;

            io::vector<Span> prim_objs{};
            if (!collect_object_ranges(json, prim_arr, prim_objs)) return false;

            for (io::usize i = 0; i < prim_objs.size(); ++i) {
                PrimitiveRec p{};
                p.mesh_index = mesh_index;
                if (!parse_int_field(json, prim_objs[i], "\"POSITION\":", p.pos_accessor)) return false;
                if (!parse_int_field(json, prim_objs[i], "\"NORMAL\":", p.normal_accessor)) return false;
                if (!parse_int_field(json, prim_objs[i], "\"TEXCOORD_0\":", p.uv_accessor)) return false;
                if (!parse_int_field(json, prim_objs[i], "\"JOINTS_0\":", p.joints_accessor)) p.joints_accessor = static_cast<io::u32>(-1);
                if (!parse_int_field(json, prim_objs[i], "\"WEIGHTS_0\":", p.weights_accessor)) p.weights_accessor = static_cast<io::u32>(-1);
                if (!parse_int_field(json, prim_objs[i], "\"indices\":", p.indices_accessor)) return false;
                if (!parse_int_field(json, prim_objs[i], "\"material\":", p.material)) p.material = 0u;
                if (!out.push_back(p)) return false;
            }
        }
        return true;
    }

    static bool build_mesh_to_node_map(const io::vector<NodeRec>& nodes,
                                       io::u32 mesh_count,
                                       io::vector<io::i32>& out_mesh_to_node) noexcept {
        out_mesh_to_node.clear();
        if (!out_mesh_to_node.resize(mesh_count)) return false;
        for (io::u32 i = 0u; i < mesh_count; ++i)
            out_mesh_to_node[i] = -1;
        for (io::u32 node_index = 0u; node_index < static_cast<io::u32>(nodes.size()); ++node_index) {
            const io::i32 mesh_index = nodes[node_index].mesh_index;
            if (mesh_index < 0) continue;
            if (static_cast<io::u32>(mesh_index) >= mesh_count) continue;
            io::i32& slot = out_mesh_to_node[static_cast<io::u32>(mesh_index)];
            if (slot < 0)
                slot = static_cast<io::i32>(node_index);
        }
        return true;
    }

    static bool parse_translation(io::char_view json, float& tx, float& ty, float& tz) noexcept {
        Span nodes{};
        if (!find_bracket_range(json, "\"nodes\":", '[', ']', nodes)) {
            tx = ty = tz = 0.f;
            return true;
        }
        io::vector<Span> node_objs{};
        if (!collect_object_ranges(json, nodes, node_objs)) return false;
        if (node_objs.empty()) {
            tx = ty = tz = 0.f;
            return true;
        }
        if (!parse_float3(json, node_objs[0], "\"translation\":", tx, ty, tz)) {
            tx = ty = tz = 0.f;
        }
        return true;
    }

    static io::u16 read_u16_le(const io::u8* p) noexcept {
        return static_cast<io::u16>(p[0] | (static_cast<io::u16>(p[1]) << 8));
    }

    static io::u32 read_u32_le(const io::u8* p) noexcept {
        return static_cast<io::u32>(p[0]
                                  | (static_cast<io::u32>(p[1]) << 8)
                                  | (static_cast<io::u32>(p[2]) << 16)
                                  | (static_cast<io::u32>(p[3]) << 24));
    }

    static float read_f32_le(const io::u8* p) noexcept {
        union U {
            io::u32 u{};
            float f;
        } u{};
        u.u = read_u32_le(p);
        return u.f;
    }

    static io::u32 accessor_stride(const AccessorRec& a, const BufferViewRec& bv) noexcept {
        if (bv.byte_stride) return bv.byte_stride;
        io::u32 elem = 0u;
        if (a.type == AccessorType::Scalar) elem = 1u;
        else if (a.type == AccessorType::Vec2) elem = 2u;
        else if (a.type == AccessorType::Vec3) elem = 3u;
        else if (a.type == AccessorType::Vec4) elem = 4u;
        else if (a.type == AccessorType::Mat4) elem = 16u;
        if (a.component_type == 5126u) return elem * 4u;
        if (a.component_type == 5125u) return elem * 4u;
        if (a.component_type == 5123u) return elem * 2u;
        return 0u;
    }

    static bool read_accessor_index(const AccessorRec& idx_acc,
                                    const BufferViewRec& idx_bv,
                                    const io::vector<io::u8>& bin,
                                    io::u32 i,
                                    io::u32& out_index) noexcept {
        const io::u32 stride = accessor_stride(idx_acc, idx_bv);
        if (stride == 0u) return false;
        io::usize off = idx_bv.byte_offset;
        if (!checked_add(off, idx_acc.byte_offset, off)) return false;
        if (!checked_add(off, static_cast<io::usize>(i) * static_cast<io::usize>(stride), off)) return false;
        if (idx_acc.component_type == 5123u) {
            if (off + 2u > bin.size()) return false;
            out_index = read_u16_le(bin.data() + off);
            return true;
        }
        if (idx_acc.component_type == 5125u) {
            if (off + 4u > bin.size()) return false;
            out_index = read_u32_le(bin.data() + off);
            return true;
        }
        return false;
    }

    static bool read_accessor_vec3(const AccessorRec& acc,
                                   const BufferViewRec& bv,
                                   const io::vector<io::u8>& bin,
                                   io::u32 index,
                                   float& x, float& y, float& z) noexcept {
        if (acc.type != AccessorType::Vec3 || acc.component_type != 5126u) return false;
        const io::u32 stride = accessor_stride(acc, bv);
        if (stride < 12u) return false;
        io::usize off = bv.byte_offset;
        if (!checked_add(off, acc.byte_offset, off)) return false;
        if (!checked_add(off, static_cast<io::usize>(index) * static_cast<io::usize>(stride), off)) return false;
        if (off + 12u > bin.size()) return false;
        x = read_f32_le(bin.data() + off + 0u);
        y = read_f32_le(bin.data() + off + 4u);
        z = read_f32_le(bin.data() + off + 8u);
        return true;
    }

    static bool read_accessor_vec2(const AccessorRec& acc,
                                   const BufferViewRec& bv,
                                   const io::vector<io::u8>& bin,
                                   io::u32 index,
                                   float& x, float& y) noexcept {
        if (acc.type != AccessorType::Vec2 || acc.component_type != 5126u) return false;
        const io::u32 stride = accessor_stride(acc, bv);
        if (stride < 8u) return false;
        io::usize off = bv.byte_offset;
        if (!checked_add(off, acc.byte_offset, off)) return false;
        if (!checked_add(off, static_cast<io::usize>(index) * static_cast<io::usize>(stride), off)) return false;
        if (off + 8u > bin.size()) return false;
        x = read_f32_le(bin.data() + off + 0u);
        y = read_f32_le(bin.data() + off + 4u);
        return true;
    }

    static bool read_accessor_vec4_f32(const AccessorRec& acc,
                                       const BufferViewRec& bv,
                                       const io::vector<io::u8>& bin,
                                       io::u32 index,
                                       float& x, float& y, float& z, float& w) noexcept {
        if (acc.type != AccessorType::Vec4 || acc.component_type != 5126u) return false;
        const io::u32 stride = accessor_stride(acc, bv);
        if (stride < 16u) return false;
        io::usize off = bv.byte_offset;
        if (!checked_add(off, acc.byte_offset, off)) return false;
        if (!checked_add(off, static_cast<io::usize>(index) * static_cast<io::usize>(stride), off)) return false;
        if (off + 16u > bin.size()) return false;
        x = read_f32_le(bin.data() + off + 0u);
        y = read_f32_le(bin.data() + off + 4u);
        z = read_f32_le(bin.data() + off + 8u);
        w = read_f32_le(bin.data() + off + 12u);
        return true;
    }

    static bool read_accessor_vec4_u16(const AccessorRec& acc,
                                       const BufferViewRec& bv,
                                       const io::vector<io::u8>& bin,
                                       io::u32 index,
                                       float& x, float& y, float& z, float& w) noexcept {
        if (acc.type != AccessorType::Vec4 || acc.component_type != 5123u) return false;
        const io::u32 stride = accessor_stride(acc, bv);
        if (stride < 8u) return false;
        io::usize off = bv.byte_offset;
        if (!checked_add(off, acc.byte_offset, off)) return false;
        if (!checked_add(off, static_cast<io::usize>(index) * static_cast<io::usize>(stride), off)) return false;
        if (off + 8u > bin.size()) return false;
        x = static_cast<float>(read_u16_le(bin.data() + off + 0u));
        y = static_cast<float>(read_u16_le(bin.data() + off + 2u));
        z = static_cast<float>(read_u16_le(bin.data() + off + 4u));
        w = static_cast<float>(read_u16_le(bin.data() + off + 6u));
        return true;
    }

    static bool read_accessor_scalar_f32(const AccessorRec& acc,
                                         const BufferViewRec& bv,
                                         const io::vector<io::u8>& bin,
                                         io::u32 index,
                                         float& out_v) noexcept {
        if (acc.type != AccessorType::Scalar || acc.component_type != 5126u) return false;
        const io::u32 stride = accessor_stride(acc, bv);
        if (stride < 4u) return false;
        io::usize off = bv.byte_offset;
        if (!checked_add(off, acc.byte_offset, off)) return false;
        if (!checked_add(off, static_cast<io::usize>(index) * static_cast<io::usize>(stride), off)) return false;
        if (off + 4u > bin.size()) return false;
        out_v = read_f32_le(bin.data() + off);
        return true;
    }

    static bool read_accessor_mat4_f32(const AccessorRec& acc,
                                       const BufferViewRec& bv,
                                       const io::vector<io::u8>& bin,
                                       io::u32 index,
                                       ge::Modeller::Float16& out_m) noexcept {
        if (acc.type != AccessorType::Mat4 || acc.component_type != 5126u) return false;
        const io::u32 stride = accessor_stride(acc, bv);
        if (stride < 64u) return false;
        io::usize off = bv.byte_offset;
        if (!checked_add(off, acc.byte_offset, off)) return false;
        if (!checked_add(off, static_cast<io::usize>(index) * static_cast<io::usize>(stride), off)) return false;
        if (off + 64u > bin.size()) return false;
        for (io::u32 i = 0; i < 16u; ++i)
            out_m.v[i] = read_f32_le(bin.data() + off + i * 4u);
        return true;
    }

    static bool read_accessor_scalar_f32_all(const AccessorRec& acc,
                                             const BufferViewRec& bv,
                                             const io::vector<io::u8>& bin,
                                             io::vector<float>& out) noexcept {
        out.clear();
        if (!out.resize(acc.count)) return false;
        for (io::u32 i = 0; i < acc.count; ++i)
            if (!read_accessor_scalar_f32(acc, bv, bin, i, out[i])) return false;
        return true;
    }

    static bool read_accessor_vec3_all(const AccessorRec& acc,
                                       const BufferViewRec& bv,
                                       const io::vector<io::u8>& bin,
                                       io::vector<ge::Modeller::Float4>& out) noexcept {
        out.clear();
        if (!out.resize(acc.count)) return false;
        for (io::u32 i = 0; i < acc.count; ++i) {
            float x = 0.f, y = 0.f, z = 0.f;
            if (!read_accessor_vec3(acc, bv, bin, i, x, y, z)) return false;
            out[i].x = x;
            out[i].y = y;
            out[i].z = z;
            out[i].w = 0.f;
        }
        return true;
    }

    static bool read_accessor_vec4_all(const AccessorRec& acc,
                                       const BufferViewRec& bv,
                                       const io::vector<io::u8>& bin,
                                       io::vector<ge::Modeller::Float4>& out) noexcept {
        out.clear();
        if (!out.resize(acc.count)) return false;
        for (io::u32 i = 0; i < acc.count; ++i) {
            float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
            if (!read_accessor_vec4_f32(acc, bv, bin, i, x, y, z, w)) return false;
            out[i].x = x;
            out[i].y = y;
            out[i].z = z;
            out[i].w = w;
        }
        return true;
    }

    static bool texture_uv_bounds(const ge::ResourceManager::TextureAtlas& atlas,
                                  io::char_view texture_name,
                                  float& u0, float& v0, float& u1, float& v1) noexcept {
        const io::u32 tid = ge::ResourceManager::texture_id_of(atlas, texture_name);
        if (tid == ge::ResourceManager::INVALID_ID) return false;
        return ge::ResourceManager::texture_uv_of(atlas, tid, u0, v0, u1, v1);
    }

    static io::char_view choose_side_axis(float nx, float ny, float nz) noexcept {
        const float ax = nx < 0.f ? -nx : nx;
        const float ay = ny < 0.f ? -ny : ny;
        const float az = nz < 0.f ? -nz : nz;
        if (ay >= ax && ay >= az) return ny > 0.f ? io::char_view{ "top" } : io::char_view{ "bottom" };
        if (ax >= az) return nx > 0.f ? io::char_view{ "right" } : io::char_view{ "left" };
        return nz > 0.f ? io::char_view{ "front" } : io::char_view{ "back" };
    }

    static bool set_text(io::string& dst, io::char_view src) noexcept {
        dst.clear();
        return dst.append(src);
    }

    static bool assign_face_texture(ge::Modeller::ImportedModel& model, io::char_view face, io::char_view tex_name) noexcept {
        if (face == "front") return set_text(model.tex_front, tex_name);
        if (face == "back") return set_text(model.tex_back, tex_name);
        if (face == "left") return set_text(model.tex_left, tex_name);
        if (face == "right") return set_text(model.tex_right, tex_name);
        if (face == "top") return set_text(model.tex_top, tex_name);
        if (face == "bottom") return set_text(model.tex_bottom, tex_name);
        return false;
    }

    static bool fill_missing_faces(ge::Modeller::ImportedModel& model) noexcept {
        io::char_view fallback{};
        if (!model.tex_front.empty()) fallback = model.tex_front.as_view();
        else if (!model.tex_back.empty()) fallback = model.tex_back.as_view();
        else if (!model.tex_left.empty()) fallback = model.tex_left.as_view();
        else if (!model.tex_right.empty()) fallback = model.tex_right.as_view();
        else if (!model.tex_top.empty()) fallback = model.tex_top.as_view();
        else if (!model.tex_bottom.empty()) fallback = model.tex_bottom.as_view();
        if (fallback.empty()) return false;

        if (model.tex_front.empty() && !set_text(model.tex_front, fallback)) return false;
        if (model.tex_back.empty() && !set_text(model.tex_back, fallback)) return false;
        if (model.tex_left.empty() && !set_text(model.tex_left, fallback)) return false;
        if (model.tex_right.empty() && !set_text(model.tex_right, fallback)) return false;
        if (model.tex_top.empty() && !set_text(model.tex_top, fallback)) return false;
        if (model.tex_bottom.empty() && !set_text(model.tex_bottom, fallback)) return false;
        return true;
    }

    static io::u32 count_u32_items(io::char_view s, Span array_range) noexcept {
        if (array_range.end <= array_range.begin + 2) return 0u;
        io::u32 count = 0u;
        io::usize at = array_range.begin + 1;
        while (at < array_range.end - 1) {
            io::u32 v = 0u;
            io::usize next = at;
            if (parse_u32(s, at, v, next)) {
                ++count;
                at = next;
                continue;
            }
            ++at;
        }
        return count;
    }

    static void make_identity(ge::Modeller::Float16& m) noexcept {
        for (io::u32 i = 0; i < 16u; ++i) m.v[i] = 0.f;
        m.v[0] = 1.f;
        m.v[5] = 1.f;
        m.v[10] = 1.f;
        m.v[15] = 1.f;
    }

    static bool parse_skeleton(io::char_view json,
                               const io::vector<AccessorRec>& accessors,
                               const io::vector<BufferViewRec>& buffer_views,
                               const io::vector<io::u8>& bin,
                               const io::vector<NodeRec>& nodes,
                               io::u32& out_skin_count,
                               io::u32& out_joint_count,
                               io::vector<ge::Modeller::Bone>& out_bones,
                               io::vector<io::i32>& out_node_to_bone) noexcept {
        out_skin_count = 0u;
        out_joint_count = 0u;
        out_bones.clear();
        out_node_to_bone.clear();
        if (!out_node_to_bone.resize(nodes.size())) return false;
        for (io::usize i = 0; i < out_node_to_bone.size(); ++i) out_node_to_bone[i] = -1;

        io::vector<io::i32> parent_node{};
        if (!parent_node.resize(nodes.size())) return false;
        for (io::usize i = 0; i < parent_node.size(); ++i) parent_node[i] = -1;
        for (io::u32 pi = 0; pi < static_cast<io::u32>(nodes.size()); ++pi) {
            const NodeRec& n = nodes[pi];
            for (io::usize ci = 0; ci < n.children.size(); ++ci) {
                const io::u32 child = n.children[ci];
                if (child < parent_node.size())
                    parent_node[child] = static_cast<io::i32>(pi);
            }
        }

        Span skins{};
        if (!find_bracket_range(json, "\"skins\":", '[', ']', skins)) {
            if (nodes.empty())
                return true;

            if (!out_bones.resize(nodes.size())) return false;
            for (io::u32 bi = 0; bi < static_cast<io::u32>(nodes.size()); ++bi) {
                ge::Modeller::Bone bone{};
                const NodeRec& n = nodes[bi];
                if (!bone.name.append(n.name.as_view())) return false;
                bone.node_index = bi;
                bone.bind_t = ge::Modeller::Float3{ n.tx, n.ty, n.tz };
                bone.bind_r = ge::Modeller::Float4{ n.rx, n.ry, n.rz, n.rw };
                bone.bind_s = ge::Modeller::Float3{ n.sx, n.sy, n.sz };
                if (parent_node[bi] >= 0 && static_cast<io::usize>(parent_node[bi]) < nodes.size())
                    bone.parent_bone = parent_node[bi];
                make_identity(bone.inverse_bind);
                out_node_to_bone[bi] = static_cast<io::i32>(bi);
                out_bones[bi] = io::move(bone);
            }
            out_joint_count = static_cast<io::u32>(out_bones.size());
            return true;
        }
        io::vector<Span> skin_objs{};
        if (!collect_object_ranges(json, skins, skin_objs)) return false;
        out_skin_count = static_cast<io::u32>(skin_objs.size());
        if (skin_objs.empty()) return true;

        io::vector<io::u32> joints{};
        if (!parse_u32_array(json, skin_objs[0], "\"joints\":", joints))
            return true;
        if (joints.empty()) return true;

        io::u32 ibm_accessor_index = static_cast<io::u32>(-1);
        (void)parse_int_field(json, skin_objs[0], "\"inverseBindMatrices\":", ibm_accessor_index);

        if (!out_bones.resize(joints.size())) return false;
        for (io::u32 bi = 0; bi < static_cast<io::u32>(joints.size()); ++bi) {
            const io::u32 node_idx = joints[bi];
            if (node_idx >= nodes.size()) continue;
            out_node_to_bone[node_idx] = static_cast<io::i32>(bi);
        }

        bool has_ibm = false;
        AccessorRec ibm_acc{};
        BufferViewRec ibm_bv{};
        if (ibm_accessor_index < accessors.size()) {
            ibm_acc = accessors[ibm_accessor_index];
            if (ibm_acc.buffer_view < buffer_views.size()) {
                ibm_bv = buffer_views[ibm_acc.buffer_view];
                has_ibm = (ibm_acc.type == AccessorType::Mat4 && ibm_acc.component_type == 5126u);
            }
        }

        for (io::u32 bi = 0; bi < static_cast<io::u32>(joints.size()); ++bi) {
            ge::Modeller::Bone bone{};
            const io::u32 node_idx = joints[bi];
            bone.node_index = node_idx;
            if (node_idx < nodes.size()) {
                const NodeRec& n = nodes[node_idx];
                if (!bone.name.append(n.name.as_view())) return false;
                bone.bind_t = ge::Modeller::Float3{ n.tx, n.ty, n.tz };
                bone.bind_r = ge::Modeller::Float4{ n.rx, n.ry, n.rz, n.rw };
                bone.bind_s = ge::Modeller::Float3{ n.sx, n.sy, n.sz };

                io::i32 p = parent_node[node_idx];
                while (p >= 0) {
                    if (static_cast<io::usize>(p) < out_node_to_bone.size() && out_node_to_bone[static_cast<io::usize>(p)] >= 0) {
                        bone.parent_bone = out_node_to_bone[static_cast<io::usize>(p)];
                        break;
                    }
                    p = (static_cast<io::usize>(p) < parent_node.size()) ? parent_node[static_cast<io::usize>(p)] : -1;
                }
            } else {
                io::StackOut<32> ss{};
                ss << "bone_" << bi;
                if (!bone.name.append(ss.view())) return false;
            }

            make_identity(bone.inverse_bind);
            if (has_ibm && bi < ibm_acc.count)
                (void)read_accessor_mat4_f32(ibm_acc, ibm_bv, bin, bi, bone.inverse_bind);
            out_bones[bi] = io::move(bone);
        }

        out_joint_count = static_cast<io::u32>(out_bones.size());
        return true;
    }

    static bool parse_animation_path(io::char_view path, ge::Modeller::AnimationPath& out_path) noexcept {
        if (path == "translation") {
            out_path = ge::Modeller::AnimationPath::Translation;
            return true;
        }
        if (path == "rotation") {
            out_path = ge::Modeller::AnimationPath::Rotation;
            return true;
        }
        if (path == "scale") {
            out_path = ge::Modeller::AnimationPath::Scale;
            return true;
        }
        return false;
    }

    static bool parse_animation_clips(io::char_view json,
                                      const io::vector<AccessorRec>& accessors,
                                      const io::vector<BufferViewRec>& buffer_views,
                                      const io::vector<io::u8>& bin,
                                      const io::vector<io::i32>& node_to_bone,
                                      io::vector<ge::Modeller::AnimationClip>& out_clips) noexcept {
        out_clips.clear();

        Span animations{};
        if (!find_bracket_range(json, "\"animations\":", '[', ']', animations))
            return true;
        io::vector<Span> anim_objs{};
        if (!collect_object_ranges(json, animations, anim_objs)) return false;

        for (io::usize i = 0; i < anim_objs.size(); ++i) {
            io::char_view anim = json.slice(anim_objs[i].begin, anim_objs[i].end - anim_objs[i].begin);
            ge::Modeller::AnimationClip clip{};

            io::char_view name{};
            if (parse_string_field(anim, Span{ 0u, anim.size() }, "\"name\":\"", name)) {
                if (!clip.name.append(name)) return false;
            } else {
                io::StackOut<32> ss{};
                ss << "clip_" << i;
                if (!clip.name.append(ss.view())) return false;
            }

            float max_time = 0.f;
            io::vector<Span> sampler_objs{};
            Span samplers{};
            if (find_bracket_range(anim, "\"samplers\":", '[', ']', samplers)) {
                if (!collect_object_ranges(anim, samplers, sampler_objs)) return false;
            }

            Span channels{};
            if (find_bracket_range(anim, "\"channels\":", '[', ']', channels)) {
                io::vector<Span> channel_objs{};
                if (!collect_object_ranges(anim, channels, channel_objs)) return false;
                for (io::usize ci = 0; ci < channel_objs.size(); ++ci) {
                    io::u32 sampler_index = static_cast<io::u32>(-1);
                    io::u32 target_node = static_cast<io::u32>(-1);
                    io::char_view target_path{};
                    if (!parse_int_field(anim, channel_objs[ci], "\"sampler\":", sampler_index)) continue;
                    if (!parse_int_field(anim, channel_objs[ci], "\"node\":", target_node)) continue;
                    if (!parse_string_field(anim, channel_objs[ci], "\"path\":\"", target_path)) continue;
                    if (target_node >= node_to_bone.size()) continue;
                    const io::i32 bone_index = node_to_bone[target_node];
                    if (bone_index < 0) continue;
                    if (sampler_index >= sampler_objs.size()) continue;

                    io::u32 input_accessor = static_cast<io::u32>(-1);
                    io::u32 output_accessor = static_cast<io::u32>(-1);
                    if (!parse_int_field(anim, sampler_objs[sampler_index], "\"input\":", input_accessor)) continue;
                    if (!parse_int_field(anim, sampler_objs[sampler_index], "\"output\":", output_accessor)) continue;
                    if (input_accessor >= accessors.size() || output_accessor >= accessors.size()) continue;

                    const AccessorRec& input_acc = accessors[input_accessor];
                    const AccessorRec& output_acc = accessors[output_accessor];
                    if (input_acc.buffer_view >= buffer_views.size() || output_acc.buffer_view >= buffer_views.size()) continue;
                    const BufferViewRec& input_bv = buffer_views[input_acc.buffer_view];
                    const BufferViewRec& output_bv = buffer_views[output_acc.buffer_view];

                    ge::Modeller::AnimationPath path{};
                    if (!parse_animation_path(target_path, path)) continue;

                    ge::Modeller::AnimationChannel channel{};
                    channel.bone_index = static_cast<io::u16>(bone_index);
                    channel.path = path;
                    if (!read_accessor_scalar_f32_all(input_acc, input_bv, bin, channel.times)) return false;

                    bool values_ok = false;
                    if (path == ge::Modeller::AnimationPath::Rotation)
                        values_ok = read_accessor_vec4_all(output_acc, output_bv, bin, channel.values);
                    else
                        values_ok = read_accessor_vec3_all(output_acc, output_bv, bin, channel.values);
                    if (!values_ok) return false;

                    const io::usize min_count = (channel.times.size() < channel.values.size()) ? channel.times.size() : channel.values.size();
                    if (min_count == 0) continue;
                    if (channel.times.size() != min_count && !channel.times.resize(min_count)) return false;
                    if (channel.values.size() != min_count && !channel.values.resize(min_count)) return false;
                    const float end_t = channel.times[min_count - 1u];
                    if (end_t > max_time) max_time = end_t;

                    if (!clip.channels.push_back(io::move(channel))) return false;
                }
            }
            clip.channel_count = static_cast<io::u32>(clip.channels.size());
            clip.duration_sec = max_time;
            if (!out_clips.push_back(io::move(clip))) return false;
        }

        return true;
    }

    static void reset_fixed_model_indices(ge::Modeller::BuildResult& build) noexcept {
        for (io::usize i = 0; i < static_cast<io::usize>(ge::Modeller::FixedModelSlot::Count); ++i)
            build.fixed_model_indices[i] = static_cast<io::usize>(-1);
    }

    static void try_assign_fixed_model_slot(ge::Modeller::BuildResult& build,
                                            io::char_view name,
                                            io::usize model_index) noexcept {
        if (name == "grass_block") {
            build.fixed_model_indices[static_cast<io::usize>(ge::Modeller::FixedModelSlot::GrassBlock)] = model_index;
            return;
        }
        if (name == "levitating_book") {
            build.fixed_model_indices[static_cast<io::usize>(ge::Modeller::FixedModelSlot::LevitatingBook)] = model_index;
            return;
        }
        if (name == "player") {
            build.fixed_model_indices[static_cast<io::usize>(ge::Modeller::FixedModelSlot::Player)] = model_index;
            return;
        }
    }

    static bool rebuild_block_descs(ge::Modeller::BuildResult& build) noexcept {
        build.block_descs.clear();
        for (io::usize i = 0; i < build.models.size(); ++i) {
            ge::Modeller::ImportedModel& m = build.models[i];
            if (m.kind != ge::Modeller::ModelKind::Block) continue;
            if (!fill_missing_faces(m)) continue;

            ge::ResourceManager::BlockDesc d{};
            d.name = m.name.as_view();
            d.textures.front = m.tex_front.as_view();
            d.textures.back = m.tex_back.as_view();
            d.textures.left = m.tex_left.as_view();
            d.textures.right = m.tex_right.as_view();
            d.textures.top = m.tex_top.as_view();
            d.textures.bottom = m.tex_bottom.as_view();
            if (!build.block_descs.push_back(d)) return false;
        }
        return true;
    }
} // namespace

namespace ge {
    io::view<const ResourceManager::BlockDesc> Modeller::BlockDescs(const BuildResult& build) noexcept {
        return io::view<const ResourceManager::BlockDesc>{ build.block_descs.data(), build.block_descs.size() };
    }

    const Modeller::ImportedModel* Modeller::FindModel(const BuildResult& build, io::char_view name) noexcept {
        for (io::usize i = 0; i < build.models.size(); ++i)
            if (build.models[i].name.as_view() == name) return &build.models[i];
        return nullptr;
    }

    io::usize Modeller::FixedModelIndex(const BuildResult& build, FixedModelSlot slot) noexcept {
        const io::usize i = static_cast<io::usize>(slot);
        if (i >= static_cast<io::usize>(FixedModelSlot::Count))
            return static_cast<io::usize>(-1);
        return build.fixed_model_indices[i];
    }

    bool Modeller::Plan(PlanResult& out_plan) noexcept {
        out_plan.items.clear();
        struct RootRec {
            io::char_view path{};
            ModelKind kind = ModelKind::Block;
        };
        const RootRec roots_to_try[] = {
            { io::char_view{ "../resources/models/blocks/" }, ModelKind::Block },
            { io::char_view{ "../../resources/models/blocks/" }, ModelKind::Block },
            { io::char_view{ "resources/models/blocks/" }, ModelKind::Block },
            { io::char_view{ "../resources/models/nature/" }, ModelKind::Block },
            { io::char_view{ "../../resources/models/nature/" }, ModelKind::Block },
            { io::char_view{ "resources/models/nature/" }, ModelKind::Block },
            { io::char_view{ "../resources/models/entities/" }, ModelKind::Entity },
            { io::char_view{ "../../resources/models/entities/" }, ModelKind::Entity },
            { io::char_view{ "resources/models/entities/" }, ModelKind::Entity },
        };

        bool found_any_root = false;
        io::vector<io::string> files{};

        for (io::usize ri = 0; ri < sizeof(roots_to_try) / sizeof(roots_to_try[0]); ++ri) {
            if (!fs::is_directory(roots_to_try[ri].path)) continue;
            found_any_root = true;
            files.clear();
            if (!collect_model_files(roots_to_try[ri].path, files)) return false;
            for (io::usize i = 0; i < files.size(); ++i) {
                io::char_view stem{};
                if (!extract_path_stem(files[i].as_view(), stem)) continue;

                PlanItem it{};
                if (!it.name.append(stem)) return false;
                if (!it.path.append(files[i].as_view())) return false;
                it.kind = roots_to_try[ri].kind;
                if (!out_plan.items.push_back(io::move(it))) return false;
            }
        }
        if (!found_any_root) return false;
        return !out_plan.items.empty();
    }

    bool Modeller::Build(const PlanResult& plan,
                         const ResourceManager::TextureAtlas& atlas,
                         BuildResult& out_build) noexcept {
        out_build.models.clear();
        out_build.block_descs.clear();
        reset_fixed_model_indices(out_build);

        io::string json_bytes{};
        io::vector<io::u8> bin{};
        io::vector<BufferViewRec> buffer_views{};
        io::vector<AccessorRec> accessors{};
        io::vector<io::string> image_names{};
        io::vector<TextureRec> textures{};
        io::vector<MaterialRec> materials{};
        io::vector<PrimitiveRec> primitives{};
        io::vector<NodeRec> nodes{};
        io::vector<io::i32> node_to_bone{};
        io::vector<io::i32> mesh_to_node{};

        for (io::usize mi = 0; mi < plan.items.size(); ++mi) {
            const PlanItem& it = plan.items[mi];
            if (!read_file_bytes(it.path.as_view(), json_bytes)) return false;
            const io::char_view json = json_bytes.as_view();

            if (!load_buffer_bytes(json, it.path.as_view(), bin)) return false;
            if (!parse_buffer_views(json, buffer_views)) return false;
            if (!parse_accessors(json, accessors)) return false;
            if (!parse_nodes(json, nodes)) return false;
            if (!parse_image_names(json, image_names)) return false;
            if (!parse_textures(json, textures)) return false;
            if (!parse_materials(json, materials)) return false;
            if (!parse_primitives(json, primitives)) return false;
            io::u32 mesh_count = 0u;
            for (io::usize pi = 0; pi < primitives.size(); ++pi) {
                const io::u32 m = primitives[pi].mesh_index + 1u;
                if (m > mesh_count) mesh_count = m;
            }
            if (!build_mesh_to_node_map(nodes, mesh_count, mesh_to_node)) return false;

            float tx = 0.f, ty = 0.f, tz = 0.f;
            if (!parse_translation(json, tx, ty, tz)) return false;

            ImportedModel model{};
            if (!model.name.append(it.name.as_view())) return false;
            if (!model.path.append(it.path.as_view())) return false;
            model.kind = it.kind;
            if (model.kind == ModelKind::Entity) {
                tx = 0.f;
                ty = 0.f;
                tz = 0.f;
            }

            if (model.kind == ModelKind::Entity) {
                if (!parse_skeleton(json, accessors, buffer_views, bin, nodes,
                                    model.skin_count, model.joint_count, model.bones, node_to_bone)) return false;
                if (!parse_animation_clips(json, accessors, buffer_views, bin, node_to_bone, model.clips)) return false;
                model.skinned = (model.skin_count > 0u && model.joint_count > 0u);
            }

            float min_x = 999999.f, min_y = 999999.f, min_z = 999999.f;
            float max_x = -999999.f, max_y = -999999.f, max_z = -999999.f;

            for (io::usize pi = 0; pi < primitives.size(); ++pi) {
                const PrimitiveRec& p = primitives[pi];
                if (p.material >= materials.size()) continue;
                const io::u32 tex_idx = materials[p.material].texture_index;
                io::char_view tex_name{};
                if (!resolve_texture_name(textures, image_names, tex_idx, tex_name)) continue;

                float au0 = 0.f, av0 = 0.f, au1 = 1.f, av1 = 1.f;
                if (!texture_uv_bounds(atlas, tex_name, au0, av0, au1, av1)) continue;

                if (p.indices_accessor >= accessors.size() || p.pos_accessor >= accessors.size() ||
                    p.normal_accessor >= accessors.size() || p.uv_accessor >= accessors.size()) continue;

                const AccessorRec& idx_acc = accessors[p.indices_accessor];
                const AccessorRec& pos_acc = accessors[p.pos_accessor];
                const AccessorRec& nrm_acc = accessors[p.normal_accessor];
                const AccessorRec& uv_acc = accessors[p.uv_accessor];
                const bool has_joint_weights = (model.kind == ModelKind::Entity
                    && p.joints_accessor != static_cast<io::u32>(-1)
                    && p.weights_accessor != static_cast<io::u32>(-1)
                    && p.joints_accessor < accessors.size()
                    && p.weights_accessor < accessors.size());
                bool has_fallback_joint = false;
                io::u16 fallback_joint = 0u;
                if (model.kind == ModelKind::Entity && !has_joint_weights &&
                    p.mesh_index < mesh_to_node.size()) {
                    const io::i32 mesh_node = mesh_to_node[p.mesh_index];
                    if (mesh_node >= 0 && static_cast<io::usize>(mesh_node) < node_to_bone.size()) {
                        const io::i32 bone_i = node_to_bone[static_cast<io::usize>(mesh_node)];
                        if (bone_i >= 0 && bone_i <= 0xFFFF) {
                            has_fallback_joint = true;
                            fallback_joint = static_cast<io::u16>(bone_i);
                        }
                    }
                }
                AccessorRec jnt_acc{};
                AccessorRec wgt_acc{};
                if (has_joint_weights) {
                    jnt_acc = accessors[p.joints_accessor];
                    wgt_acc = accessors[p.weights_accessor];
                }

                if (idx_acc.buffer_view >= buffer_views.size() || pos_acc.buffer_view >= buffer_views.size() ||
                    nrm_acc.buffer_view >= buffer_views.size() || uv_acc.buffer_view >= buffer_views.size()) continue;
                if (has_joint_weights &&
                    (jnt_acc.buffer_view >= buffer_views.size() || wgt_acc.buffer_view >= buffer_views.size()))
                    continue;

                const BufferViewRec& idx_bv = buffer_views[idx_acc.buffer_view];
                const BufferViewRec& pos_bv = buffer_views[pos_acc.buffer_view];
                const BufferViewRec& nrm_bv = buffer_views[nrm_acc.buffer_view];
                const BufferViewRec& uv_bv = buffer_views[uv_acc.buffer_view];
                BufferViewRec jnt_bv{};
                BufferViewRec wgt_bv{};
                if (has_joint_weights) {
                    jnt_bv = buffer_views[jnt_acc.buffer_view];
                    wgt_bv = buffer_views[wgt_acc.buffer_view];
                }

                bool face_mapped = false;
                for (io::u32 ii = 0; ii < idx_acc.count; ++ii) {
                    io::u32 vx_idx = 0u;
                    if (!read_accessor_index(idx_acc, idx_bv, bin, ii, vx_idx)) return false;
                    if (vx_idx >= pos_acc.count || vx_idx >= nrm_acc.count || vx_idx >= uv_acc.count) return false;

                    float px = 0.f, py = 0.f, pz = 0.f;
                    float nx = 0.f, ny = 1.f, nz = 0.f;
                    float uu = 0.f, vv = 0.f;
                    float j0 = 0.f, j1 = 0.f, j2 = 0.f, j3 = 0.f;
                    float w0 = 1.f, w1 = 0.f, w2 = 0.f, w3 = 0.f;
                    if (!read_accessor_vec3(pos_acc, pos_bv, bin, vx_idx, px, py, pz)) return false;
                    if (!read_accessor_vec3(nrm_acc, nrm_bv, bin, vx_idx, nx, ny, nz)) return false;
                    if (!read_accessor_vec2(uv_acc, uv_bv, bin, vx_idx, uu, vv)) return false;
                    if (model.kind == ModelKind::Entity)
                        vv = 1.f - vv;
                    if (has_joint_weights) {
                        if (!read_accessor_vec4_u16(jnt_acc, jnt_bv, bin, vx_idx, j0, j1, j2, j3)) return false;
                        if (!read_accessor_vec4_f32(wgt_acc, wgt_bv, bin, vx_idx, w0, w1, w2, w3)) return false;
                    } else if (has_fallback_joint) {
                        j0 = static_cast<float>(fallback_joint);
                        j1 = j2 = j3 = 0.f;
                        w0 = 1.f;
                        w1 = w2 = w3 = 0.f;
                    }

                    px += tx; py += ty; pz += tz;
                    if (px < min_x) min_x = px;
                    if (py < min_y) min_y = py;
                    if (pz < min_z) min_z = pz;
                    if (px > max_x) max_x = px;
                    if (py > max_y) max_y = py;
                    if (pz > max_z) max_z = pz;

                    if (!face_mapped) {
                        const io::char_view face = choose_side_axis(nx, ny, nz);
                        if (!assign_face_texture(model, face, tex_name)) return false;
                        face_mapped = true;
                    }

                    MeshVertex out_v{};
                    out_v.px = px;
                    out_v.py = py;
                    out_v.pz = pz;
                    out_v.nx = nx;
                    out_v.ny = ny;
                    out_v.nz = nz;
                    out_v.u = au0 + uu * (au1 - au0);
                    out_v.v = av0 + vv * (av1 - av0);
                    out_v.j0 = j0;
                    out_v.j1 = j1;
                    out_v.j2 = j2;
                    out_v.j3 = j3;
                    out_v.w0 = w0;
                    out_v.w1 = w1;
                    out_v.w2 = w2;
                    out_v.w3 = w3;
                    if (!model.vertices.push_back(out_v)) return false;
                    if (!model.indices.push_back(static_cast<io::u32>(model.vertices.size() - 1u))) return false;
                }
            }

            const float sx = max_x - min_x;
            const float sy = max_y - min_y;
            const float sz = max_z - min_z;
            model.cube_like = (sx > 0.95f && sx < 1.05f && sy > 0.95f && sy < 1.05f && sz > 0.95f && sz < 1.05f);

            if (model.vertices.empty() || model.indices.empty()) continue;
            if (!out_build.models.push_back(io::move(model))) return false;
            try_assign_fixed_model_slot(out_build, it.name.as_view(), out_build.models.size() - 1u);
        }

        if (out_build.models.empty()) return false;
        return rebuild_block_descs(out_build);
    }
} // namespace ge
