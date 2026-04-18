#pragma once

struct Window;

namespace ge {
namespace client {
namespace render {
    struct ScenePipeline final {
        static void RenderFrame(Window& win, float dt) noexcept;
    };
} // namespace render
} // namespace client
} // namespace ge
