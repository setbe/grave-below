#define IO_IMPLEMENTATION
#include "hi/hi/hi.hpp"

#include "../../../engine/core/config.hpp"
#include "../../../engine/core/resource_manager.hpp"

static bool expect_true(bool cond, io::char_view msg) noexcept {
    if (cond) return true;
    io::out << "[FAIL] " << msg << '\n';
    return false;
}

int main() {
    ge::ConfigBinary cfg{};
    (void)ge::ensure_config_binary(cfg);

    ge::ResourceManager::TextureAtlas atlas{};
    ge::ResourceManager::TextureAtlasOptions options{};
    options.desired_channels = 4;
    options.atlas_padding_px = 1;
    options.max_blocks = 128;

    if (!ge::ResourceManager::texture_atlas_from(atlas, options)) {
        io::out << "[FAIL] texture_atlas_from() failed\n";
        return 1;
    }

    io::out << "[atlas] textures=" << atlas.texture_count
            << " size=" << atlas.atlas_width << "x" << atlas.atlas_height
            << " channels=" << (int)atlas.atlas_channels
            << " decode_buf=" << atlas.decode_pixels_size
            << " scratch=" << atlas.scratch_size << '\n';

    if (!expect_true(atlas.texture_count > 0, "atlas.texture_count > 0")) return 2;
    if (!expect_true(atlas.atlas_width > 0 && atlas.atlas_height > 0, "atlas dimensions > 0")) return 3;
    if (!expect_true(atlas.atlas_width == atlas.atlas_height, "atlas is square")) return 4;
    if (!expect_true((atlas.atlas_width & (atlas.atlas_width - 1u)) == 0u, "atlas side is power-of-two")) return 5;

    for (io::u32 i = 0; i < atlas.texture_count; ++i) {
        const io::char_view name = ge::ResourceManager::texture_name_of(atlas, i);
        if (!expect_true(!name.empty(), "texture_name_of is non-empty")) return 6;

        float u0 = 0.f;
        float v0 = 0.f;
        float u1 = 0.f;
        float v1 = 0.f;
        if (!expect_true(ge::ResourceManager::texture_uv_of(atlas, i, u0, v0, u1, v1), "texture_uv_of returns true")) return 7;
        if (!expect_true(u0 >= 0.f && v0 >= 0.f && u1 <= 1.f && v1 <= 1.f, "UV range [0..1]")) return 8;
        if (!expect_true(u1 > u0 && v1 > v0, "UV extents are positive")) return 9;

        io::out << "  tex[" << i << "] name='" << name
                << "' uv=(" << u0 << "," << v0 << ")..(" << u1 << "," << v1 << ")\n";
    }

    const ge::ResourceManager::BlockDesc blocks[] = {
        { "stone", ge::ResourceManager::faces_all("stone") },
        { "grass_block", ge::ResourceManager::faces_top_bottom_side("grass_up", "dirt", "grass_side") },
        { "snowy_furnace_like", ge::ResourceManager::faces_furnace("snow", "stone", "grass_up", "dirt") },
    };

    if (!ge::ResourceManager::register_blocks(atlas, io::view<const ge::ResourceManager::BlockDesc>{ blocks, 3 })) {
        io::out << "[FAIL] register_blocks() failed\n";
        return 10;
    }

    const io::u32 id_stone = ge::ResourceManager::block_id_of(atlas, "stone");
    const io::u32 id_grass = ge::ResourceManager::block_id_of(atlas, "grass_block");
    const io::u32 id_snowy = ge::ResourceManager::block_id_of(atlas, "snowy_furnace_like");

    if (!expect_true(id_stone != ge::ResourceManager::INVALID_ID, "stone id exists")) return 11;
    if (!expect_true(id_grass != ge::ResourceManager::INVALID_ID, "grass_block id exists")) return 12;
    if (!expect_true(id_snowy != ge::ResourceManager::INVALID_ID, "snowy_furnace_like id exists")) return 13;

    io::out << "[atlas] blocks=" << atlas.block_count << '\n';
    io::out << "  block stone id=" << id_stone << '\n';
    io::out << "  block grass_block id=" << id_grass << '\n';
    io::out << "  block snowy_furnace_like id=" << id_snowy << '\n';

    io::out << "test_texture_atlas: ok\n";
    return 0;
}
