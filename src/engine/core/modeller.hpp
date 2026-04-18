#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"

#include "resource_manager.hpp"

namespace ge {
    struct Modeller {
        enum class FixedModelSlot : io::u8 {
            GrassBlock = 0,
            LevitatingBook = 1,
            Player = 2,
            Count = 3
        };

        enum class ModelKind : io::u8 {
            Block = 0,
            Entity = 1
        };

        enum class AnimationPath : io::u8 {
            Translation = 0,
            Rotation = 1,
            Scale = 2
        };

        struct Float3 {
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
        };

        struct Float4 {
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
            float w = 0.f;
        };

        struct Float16 {
            float v[16]{};
        };

        struct Bone {
            io::string name{};
            io::u32 node_index = 0u;
            io::i32 parent_bone = -1;
            Float3 bind_t{};
            Float4 bind_r{ 0.f, 0.f, 0.f, 1.f };
            Float3 bind_s{ 1.f, 1.f, 1.f };
            Float16 inverse_bind{};
        };

        struct AnimationChannel {
            io::u16 bone_index = 0u;
            AnimationPath path = AnimationPath::Rotation;
            io::vector<float> times{};
            io::vector<Float4> values{};
        };

        struct AnimationClip {
            io::string name{};
            float duration_sec = 0.f;
            io::u32 channel_count = 0u;
            io::vector<AnimationChannel> channels{};
        };

        struct MeshVertex {
            float px{};
            float py{};
            float pz{};
            float nx{};
            float ny{};
            float nz{};
            float u{};
            float v{};
            float j0{};
            float j1{};
            float j2{};
            float j3{};
            float w0{ 1.f };
            float w1{};
            float w2{};
            float w3{};
        };

        struct ImportedModel {
            io::string name{};
            io::string path{};
            ModelKind kind = ModelKind::Block;

            io::vector<MeshVertex> vertices{};
            io::vector<io::u32> indices{};

            io::string tex_front{};
            io::string tex_back{};
            io::string tex_left{};
            io::string tex_right{};
            io::string tex_top{};
            io::string tex_bottom{};

            bool cube_like{};
            bool skinned = false;
            io::u32 skin_count = 0u;
            io::u32 joint_count = 0u;
            io::vector<Bone> bones{};
            io::vector<AnimationClip> clips{};
        };

        struct PlanItem {
            io::string name{};
            io::string path{};
            ModelKind kind = ModelKind::Block;
        };

        struct PlanResult {
            io::vector<PlanItem> items{};
        };

        struct BuildResult {
            io::vector<ImportedModel> models{};
            io::vector<ResourceManager::BlockDesc> block_descs{};
            io::usize fixed_model_indices[static_cast<io::usize>(FixedModelSlot::Count)]{
                static_cast<io::usize>(-1), static_cast<io::usize>(-1), static_cast<io::usize>(-1)
            };
        };

        static bool Plan(PlanResult& out_plan) noexcept;
        static bool Build(const PlanResult& plan,
                          const ResourceManager::TextureAtlas& atlas,
                          BuildResult& out_build) noexcept;

        IO_NODISCARD static io::view<const ResourceManager::BlockDesc> BlockDescs(const BuildResult& build) noexcept;
        IO_NODISCARD static const ImportedModel* FindModel(const BuildResult& build, io::char_view name) noexcept;
        IO_NODISCARD static io::usize FixedModelIndex(const BuildResult& build, FixedModelSlot slot) noexcept;
    };
} // namespace ge

#ifdef GE_MODELLER_IMPLEMENTATION
#include "modeller.cpp"
#endif
