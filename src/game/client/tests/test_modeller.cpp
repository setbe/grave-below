#define IO_IMPLEMENTATION
#include "hi/hi/hi.hpp"

#define GE_MODELLER_IMPLEMENTATION
#include "../../../engine/core/modeller.hpp"
#include "../../../engine/core/resource_manager.hpp"

int main() {
    ge::ResourceManager::TextureAtlas atlas{};
    ge::ResourceManager::TextureAtlasOptions opt{};
    opt.desired_channels = 4;
    opt.atlas_padding_px = 1;
    opt.max_blocks = 512;
    if (!ge::ResourceManager::texture_atlas_from(atlas, opt)) {
        io::out << "atlas failed\n";
        return 1;
    }

    ge::Modeller::PlanResult plan{};
    if (!ge::Modeller::Plan(plan)) {
        io::out << "plan failed\n";
        return 2;
    }
    io::out << "plan items=" << plan.items.size() << '\n';

    ge::Modeller::BuildResult build{};
    if (!ge::Modeller::Build(plan, atlas, build)) {
        io::out << "build failed\n";
        return 3;
    }
    io::out << "models=" << build.models.size() << " block_descs=" << build.block_descs.size() << '\n';

    const io::view<const ge::ResourceManager::BlockDesc> descs = ge::Modeller::BlockDescs(build);
    if (descs.empty()) {
        io::out << "block descs empty\n";
        return 4;
    }
    if (!ge::ResourceManager::register_blocks(atlas, descs)) {
        io::out << "register_blocks failed\n";
        return 5;
    }
    io::out << "registered blocks=" << atlas.block_count << '\n';

    const ge::Modeller::ImportedModel* grass = ge::Modeller::FindModel(build, "grass_block");
    if (!grass) {
        io::out << "grass_block not found\n";
        return 6;
    }
    if (grass->tex_top.as_view() != "grass_up" || grass->tex_bottom.as_view() != "dirt") {
        io::out << "grass_block face textures mismatch\n";
        return 7;
    }
    if (grass->tex_front.as_view() != "grass_side" || grass->tex_left.as_view() != "grass_side") {
        io::out << "grass_block side textures mismatch\n";
        return 8;
    }

    const ge::Modeller::ImportedModel* dirt = ge::Modeller::FindModel(build, "dirt");
    const ge::Modeller::ImportedModel* sand = ge::Modeller::FindModel(build, "sand");
    const ge::Modeller::ImportedModel* stone = ge::Modeller::FindModel(build, "stone");
    if (!dirt || dirt->tex_front.as_view() != "dirt") return 9;
    if (!sand || sand->tex_front.as_view() != "sand") return 10;
    if (!stone) return 11;
    if (!(stone->tex_front.as_view() == "stone" || stone->tex_front.as_view() == "cobblestone")) return 11;

    const ge::Modeller::ImportedModel* book = ge::Modeller::FindModel(build, "levitating_book");
    if (!book) {
        io::out << "levitating_book not found\n";
        return 12;
    }
    if (book->kind != ge::Modeller::ModelKind::Entity) {
        io::out << "levitating_book kind mismatch\n";
        return 13;
    }
    if (!book->skinned) {
        io::out << "levitating_book should be skinned\n";
        return 14;
    }
    if (book->bones.empty()) {
        io::out << "levitating_book bones missing\n";
        return 16;
    }
    if (book->clips.empty()) {
        io::out << "levitating_book clips missing\n";
        return 15;
    }
    if (book->clips[0].channels.empty()) {
        io::out << "levitating_book clip channels missing\n";
        return 17;
    }
    io::out << "levitating_book joints=" << book->joint_count
            << " skins=" << book->skin_count
            << " clips=" << book->clips.size() << '\n';
    for (io::usize i = 0; i < book->clips.size(); ++i)
        io::out << "  clip[" << i << "] name=" << book->clips[i].name
                << " duration=" << book->clips[i].duration_sec
                << " channels=" << book->clips[i].channel_count << '\n';

    io::out << "grass vertices=" << grass->vertices.size() << " indices=" << grass->indices.size() << '\n';
    io::out << "test_modeller: ok\n";
    return 0;
}
