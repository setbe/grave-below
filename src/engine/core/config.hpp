#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"

namespace ge {
    // ------------------------------------------------------------------------
    // GUI
    // ------------------------------------------------------------------------

    static constexpr hi::TextStyle BUTTON_STYLE_NORMAL{ 1.f,  1.f,  1.f,  1.f, false };
    static constexpr hi::TextStyle BUTTON_STYLE_HOVER{  1.f,  1.f,  1.f,  1.f, true, 0.f, 0.f, 0.f, 1.f, /*.outline_px*/0.9f, /*.softness_px*/0.9f };
    static constexpr hi::TextStyle BUTTON_STYLE_ACTIVE{ 0.f,  0.f,  0.f,  1.f, true, 1.f, 1.f, 1.f, 1.f, /*.outline_px*/1.5f, /*.softness_px*/0.9f };

    static constexpr float FONT_PIXEL_HEIGHT = 24.f;

    // ------------------------------------------------------------------------
    // Strings
    // ------------------------------------------------------------------------
    static io::char_view PATH_RESOURCES{ "../resources/" };
    static io::char_view PATH_SHADERS{ "shaders/" };
    static io::char_view PATH_FONTS_TTF{ "fonts ttf/" };

    static io::char_view WINDOW_TITLE{ "Grave Below" };

    static io::char_view FILENAME_LOG{ "log.txt" };
    static io::char_view FILENAME_WORLD_FONT{ "Monocraft.ttf" };
    

} // namespace ge