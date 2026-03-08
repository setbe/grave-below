#define IO_IMPLEMENTATION
#include "hi/hi/hi.hpp"

#include "../../engine/core/config.hpp"
#include "../../engine/core/logger.hpp"
#include "../../engine/core/resource_manager.hpp"


static constexpr float vertices[] = {
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.0f,  0.5f, 0.0f
};

struct MainWindow : public hi::Window<MainWindow> {
    // --- ui ---
    io::u64 last_click_ms = io::monotonic_ms();
    // --- gl ---
    hi::AtlasId world_atlas{ -1 };
    
    gl::Buffer vbo{ gl::BufferTarget::ArrayBuffer };
    gl::VertexArray vao{};
    gl::Shader fill_orange{};


    static constexpr int dt_hist_cap = 256;

    float dt_hist[dt_hist_cap]{};
    int   dt_hist_index = 0;
    int   dt_hist_count = 0;
    float dt_hist_sum = 0.f;
    
    bool wireframe_mode = false;

    inline void ResetDtHistory() noexcept {
        for (int i = 0; i < dt_hist_cap; ++i) dt_hist[i] = 0.f;
        dt_hist_index = 0;
        dt_hist_count = 0;
        dt_hist_sum = 0.f;
    }

    MainWindow() noexcept {
        this->setTitle(ge::WINDOW_TITLE);
        this->setElementScale(2.f);
        this->setTargetFps(0);
        gl::Viewport(0, 0, width(), height());

        // --- gl ---
        vbo.bind();
        vbo.data(vertices, sizeof(vertices), gl::BufferUsage::StaticDraw);

        gl::Attr attrs[]{
            gl::AttrOf<float>(3, false),
        };
        gl::MeshBinder::setup(vao, vbo, { attrs, sizeof(attrs) / sizeof(attrs[0]) });
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
        case K::_1: setCursorVisible(!isCursorVisible());
            break;
        case K::_2: setFullscreen(!isFullscreen());
            break;
        
        case K::_3: {
            // be careful on OpenGL ES (Android),
            // because OpenGL Core has `gl::PolygonMode` function only
            wireframe_mode = !wireframe_mode;
            gl::PolygonMode(gl::Face::FrontAndBack,
                wireframe_mode ? gl::Polygon::Line : gl::Polygon::Fill);
            break;
        }
        case K::_4:
            setVSyncEnable(!isVSync());
            ResetDtHistory();
            break;
        } // switch
    }

    void onMouseMove(int x, int y) noexcept override {
    }


    void onRender(float dt) noexcept override {
        gl::ClearColor(0.33f, 0.33f, 0.33f, 0.f);
        gl::Clear(gl::buffer_bit.Color | gl::buffer_bit.Depth);

        fill_orange.Use();
        vao.bind();
        gl::DrawArrays(gl::PrimitiveMode::Triangles, 0, 3);

        RenderGui(dt);
    }



    void RenderGui(float dt) noexcept {
        static constexpr float paragraph_spacing = ge::FONT_PIXEL_HEIGHT + 14.f;
        
        hi::TextDraw td{};
        td.atlas = world_atlas;
        td.dock = hi::TextDock::TopR;

        const float avg_fps = PushDtSample(dt);

        io::StackOut<512> text_ss{};
        text_ss.reset();
        text_ss << "FPS(avg): " << avg_fps << '\n'
                << "VSync: " << isVSync();
        td.text = text_ss.view();
        this->DrawText(td);

        auto st = ButtonRegular("Click ME!!!!");
        auto st2 = ButtonRegular("no you should click me instead", 0.f, 1.f*paragraph_spacing);
        if (st.clicked) {
            setTitle(IO_U8("Clicked!"));
            last_click_ms = io::monotonic_ms();
        }

        FlushText();
    }

    inline float PushDtSample(float dt) noexcept {
        dt_hist_sum -= dt_hist[dt_hist_index];
        dt_hist[dt_hist_index] = dt;
        dt_hist_sum += dt;

        ++dt_hist_index;
        if (dt_hist_index >= dt_hist_cap) dt_hist_index = 0;
        if (dt_hist_count < dt_hist_cap) ++dt_hist_count;

        const float avg_dt = (dt_hist_count > 0) ? (dt_hist_sum / (float)dt_hist_count) : 0.f;
        return (avg_dt > 0.000001f) ? (1.f / avg_dt) : 0.f;
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
        return LoadFonts() && LoadShaders();
    }

    bool LoadShaders() noexcept {
        return ge::ResourceManager::shader_from("fill_orange.frag", "fill_orange.vert", fill_orange);
    }

    bool LoadFonts() noexcept {
        hi::FontAtlasDesc desc{};
        desc.mode = hi::FontAtlasMode::SDF;
        desc.pixel_height = static_cast<io::u16>(ge::FONT_PIXEL_HEIGHT);
        desc.spread_px = 2.f;

        hi::FontId font_id{ -1 };
        {
            io::StackOut<220> ss{};
            ss << ge::PATH_RESOURCES << ge::PATH_FONTS_TTF << ge::FILENAME_WORLD_FONT;
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

        if (win.last_click_ms < io::monotonic_ms() - 1500)
            win.setTitle(ge::WINDOW_TITLE);
    }
    io::exit_process(0);
}