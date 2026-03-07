#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"

namespace ge {
    // ------------------------------------------------------------------------
    // GUI
    // ------------------------------------------------------------------------

    hi::TextStyle BUTTON_STYLE_NORMAL{ 1.f,  1.f,  1.f,  1.f, false };
    hi::TextStyle BUTTON_STYLE_HOVER{  0.7f, 0.7f, 0.7f, 1.f, true, 0.f, 0.f, 0.f, 1.f, /*.outline_px*/0.7f, /*.softness_px*/0.9f };
    hi::TextStyle BUTTON_STYLE_ACTIVE{ 0.f,  0.f,  0.f,  1.f, true, 1.f, 1.f, 1.f, 1.f, /*.outline_px*/1.5f, /*.softness_px*/1.3f };

    // ------------------------------------------------------------------------
    // Strings
    // ------------------------------------------------------------------------
    static io::char_view RESOURCE_PATH{ "../resources/" };

    static io::char_view WINDOW_TITLE{ "Grave Below" };

    static io::char_view LOG_FILENAME{ "log.txt" };
    static io::char_view WORLD_FONT_PATH{ "monocraft/Monocraft.ttf" };
    

} // namespace ge