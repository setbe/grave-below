#pragma once

struct Window;

namespace ge {
namespace client {
namespace render {
    struct UiPipeline final {
        static void Render(Window& win, float dt) noexcept;
    };
} // namespace render
} // namespace client
} // namespace ge
