#define IO_IMPLEMENTATION
#include "hi/hi/hi.hpp"

#include "../../engine/core/config.hpp"
#include "../../engine/core/logger.hpp"
#include "../../engine/core/resource_manager.hpp"




struct MainWindow : public hi::Window<MainWindow> {
    hi::AtlasId world_atlas{ -1 };
    io::u64 last_click_ms = io::monotonic_ms();

    MainWindow() noexcept {
        this->setTitle(ge::WINDOW_TITLE);
        this->setElementScale(4.f);
    }

    void onError(hi::Error err, hi::AboutError ae) noexcept override {
        setTitle(hi::what(ae));
    }

    void onKeyDown(hi::Key k) noexcept override {
        using K = hi::Key;
    }

    void onKeyUp(hi::Key k) noexcept override {
        using K = hi::Key;
        switch (k) {
        case K::_1: setCursorVisible(isCursorVisible());  break;
        case K::_2: setFullscreen(isFullscreen()); break;
        }
    }

    void onMouseMove(int x, int y) noexcept override {
    }

    void onRender() noexcept override {
        gl::ClearColor(0.33f, 0.33f, 0.33f, 0.f);
        gl::Clear(gl::buffer_bit.Color | gl::buffer_bit.Depth);

        auto st = ButtonRegular("Click ME!!!!");
        if (st.clicked) {
            setTitle(IO_U8("Clicked!"));
            last_click_ms = io::monotonic_ms();
        }

        this->FlushText();
    }

    inline hi::ButtonState ButtonRegular(io::char_view text, hi::TextDock dock = hi::TextDock::TopL) noexcept {
        return ButtonRegular(text, 0.f, 0.f, dock);
    }
    inline hi::ButtonState ButtonRegular(io::char_view text, float x, float y, hi::TextDock dock = hi::TextDock::TopL) noexcept {
        hi::ButtonDraw btn{};
        btn.atlas = world_atlas;
        btn.dock = dock;
        btn.x = x; btn.y = y;
        btn.space_between = -0.3f;
        btn.text = text;
        btn.style.normal = ge::BUTTON_STYLE_NORMAL;
        btn.style.hover = ge::BUTTON_STYLE_HOVER;
        btn.style.active = ge::BUTTON_STYLE_ACTIVE;
        return Button(btn);
    }

    bool LoadResources() noexcept {
        return LoadFonts();
    }

    bool LoadFonts() noexcept {
        hi::FontAtlasDesc desc{};
        desc.mode = hi::FontAtlasMode::SDF;
        desc.pixel_height = 24;
        desc.spread_px = 2.f;

        hi::FontId font_id{ -1 };
        {
            io::StackOut<220> ss{};
            ss << ge::RESOURCE_PATH << ge::WORLD_FONT_PATH;
            font_id = this->LoadFont(ss.view());
        }
        if (font_id < 0) return false;
        world_atlas = this->GenerateFontAtlas(font_id, desc,
            stbtt_codepoints::Script::Latin,
            stbtt_codepoints::Script::Cyrillic);
        return world_atlas >= 0;
    }
}; // struct MainWindow

int main() {
    MainWindow win{};
    if (!win.LoadResources()) io::exit_process(-1);

    while (win.PollEvents()) {
        win.Render();
        win.SwapBuffers();

        if (win.last_click_ms < io::monotonic_ms() - 1500)
            win.setTitle(ge::WINDOW_TITLE);
    }
    io::exit_process(0);
}