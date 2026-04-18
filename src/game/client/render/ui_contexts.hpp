#include "../context/main_menu.hpp"
#include "../context/multiplayer.hpp"
#include "../context/connecting.hpp"
#include "../context/settings.hpp"
#include "../context/graphics.hpp"
#include "../context/key_bindings.hpp"
#include "../context/in_game.hpp"
#include "../context/in_game_dead.hpp"

    inline void RenderGui(float dt) noexcept {
        ge::client::render::UiPipeline::Render(*this, dt);
    }

