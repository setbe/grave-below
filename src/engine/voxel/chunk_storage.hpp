#pragma once

#include "../core/config.hpp"
#include "chunk.hpp"

namespace ge {
namespace voxel {
    IO_NODISCARD static inline io::spin_mutex& chunk_storage_lock() noexcept {
        static io::spin_mutex lock{};
        return lock;
    }

    struct ChunkDiskHeader {
        io::u32 magic = 0x4B4E4843u; // "CHNK"
        io::u32 version = 1u;
        io::i32 cx{};
        io::i32 cy{};
        io::i32 cz{};
        io::u32 non_air_count{};
    };

    IO_NODISCARD static inline bool resolve_resources_root(io::string& out_root) noexcept {
        const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (!fs::is_directory(roots[i])) continue;
            out_root.clear();
            return out_root.append(roots[i]);
        }
        out_root.clear();
        return out_root.append(PATH_RESOURCES);
    }

    template<io::usize N>
    IO_NODISCARD static inline bool resolve_resources_root_stack(io::StackOut<N>& out_root) noexcept {
        out_root.reset();
        const io::char_view roots[] = { PATH_RESOURCES, PATH_RESOURCES_ALT_1, PATH_RESOURCES_ALT_2 };
        for (io::usize i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (!fs::is_directory(roots[i])) continue;
            out_root << roots[i];
            return true;
        }
        out_root << PATH_RESOURCES;
        return true;
    }

    IO_NODISCARD static inline bool ensure_chunk_storage_root(io::string& out_root) noexcept {
        io::string root{};
        if (!resolve_resources_root(root)) return false;

        io::string world_dir{};
        if (!fs::path_join(root.as_view(), "world", world_dir)) return false;
        if (!fs::is_directory(world_dir) && !fs::create_directory(world_dir))
            return false;

        out_root.clear();
        if (!fs::path_join(world_dir.as_view(), "chunks", out_root)) return false;
        if (!fs::is_directory(out_root) && !fs::create_directory(out_root))
            return false;
        return true;
    }

    template<io::usize N>
    IO_NODISCARD static inline bool ensure_chunk_storage_root_stack(io::StackOut<N>& out_root) noexcept {
        io::StackOut<N> root{};
        if (!resolve_resources_root_stack(root)) return false;

        io::StackOut<N> world_dir{};
        fs::path_join(root.view(), "world", world_dir);
        if (world_dir.view().empty()) return false;
        if (!fs::is_directory(world_dir.view()) && !fs::create_directory(world_dir.view()))
            return false;

        out_root.reset();
        fs::path_join(world_dir.view(), "chunks", out_root);
        if (out_root.view().empty()) return false;
        if (!fs::is_directory(out_root.view()) && !fs::create_directory(out_root.view()))
            return false;
        return true;
    }

    static inline void chunk_file_name(const ChunkCoord& coord, io::StackOut<96>& out) noexcept {
        out.reset();
        out << "c_" << coord.x << "_" << coord.y << "_" << coord.z << ".bin";
    }

    IO_NODISCARD static inline bool build_chunk_file_path(const ChunkCoord& coord, io::string& out_path) noexcept {
        io::string chunks_root{};
        if (!ensure_chunk_storage_root(chunks_root)) return false;
        io::StackOut<96> leaf{};
        chunk_file_name(coord, leaf);
        return fs::path_join(chunks_root.as_view(), leaf.view(), out_path);
    }

    template<io::usize N>
    IO_NODISCARD static inline bool build_chunk_file_path_stack(const ChunkCoord& coord, io::StackOut<N>& out_path) noexcept {
        io::StackOut<N> chunks_root{};
        if (!ensure_chunk_storage_root_stack(chunks_root)) return false;
        io::StackOut<96> leaf{};
        chunk_file_name(coord, leaf);
        out_path.reset();
        fs::path_join(chunks_root.view(), leaf.view(), out_path);
        return !out_path.view().empty();
    }

    IO_NODISCARD static inline bool save_chunk_binary(const ChunkData& chunk) noexcept {
        io::spin_mutex& lock = chunk_storage_lock();
        lock.lock();
        io::StackOut<384> path{};
        if (!build_chunk_file_path_stack(chunk.coord, path)) {
            lock.unlock();
            return false;
        }

        fs::File f{ path.view(), io::OpenMode::Write | io::OpenMode::Create | io::OpenMode::Truncate | io::OpenMode::Binary };
        if (!f.is_open()) {
            lock.unlock();
            return false;
        }

        ChunkDiskHeader h{};
        h.cx = chunk.coord.x;
        h.cy = chunk.coord.y;
        h.cz = chunk.coord.z;
        h.non_air_count = chunk.non_air_count;

        const io::char_view hbytes{ reinterpret_cast<const char*>(&h), sizeof(h) };
        if (f.write(hbytes) != hbytes.size()) {
            lock.unlock();
            return false;
        }

        const io::char_view bbytes{ reinterpret_cast<const char*>(chunk.blocks), sizeof(BlockState) * CHUNK_VOLUME };
        if (f.write(bbytes) != bbytes.size()) {
            lock.unlock();
            return false;
        }
        const bool ok = f.flush();
        lock.unlock();
        return ok;
    }

    IO_NODISCARD static inline bool load_chunk_binary(const ChunkCoord& coord, ChunkData& out_chunk) noexcept {
        io::spin_mutex& lock = chunk_storage_lock();
        lock.lock();
        io::StackOut<384> path{};
        if (!build_chunk_file_path_stack(coord, path)) {
            lock.unlock();
            return false;
        }
        if (!fs::exists(path.view())) {
            lock.unlock();
            return false;
        }

        fs::File f{ path.view(), io::OpenMode::Read | io::OpenMode::Binary };
        if (!f.is_open()) {
            lock.unlock();
            return false;
        }

        ChunkDiskHeader h{};
        if (!f.read_exact(&h, sizeof(h))) {
            lock.unlock();
            return false;
        }
        if (h.magic != 0x4B4E4843u) {
            lock.unlock();
            return false;
        }
        if (h.version != 1u) {
            lock.unlock();
            return false;
        }
        if (h.cx != coord.x || h.cy != coord.y || h.cz != coord.z) {
            lock.unlock();
            return false;
        }
        if (!f.read_exact(out_chunk.blocks, sizeof(BlockState) * CHUNK_VOLUME)) {
            lock.unlock();
            return false;
        }
        lock.unlock();

        out_chunk.coord = coord;
        out_chunk.non_air_count = 0u;
        for (io::u32 i = 0; i < CHUNK_VOLUME; ++i) {
            if (out_chunk.blocks[i].id >= BLOCK_COUNT) {
                out_chunk.blocks[i].id = block_index(BlockId::Air);
                out_chunk.blocks[i].state = 0u;
            }
            if (out_chunk.blocks[i].id != block_index(BlockId::Air))
                ++out_chunk.non_air_count;
        }
        out_chunk.version = 0u;
        out_chunk.dirty_mesh = false;
        out_chunk.dirty_neighbors = false;
        out_chunk.generated = true;
        return true;
    }
} // namespace voxel
} // namespace ge
