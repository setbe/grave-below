#define IO_IMPLEMENTATION
#include "hi/hi/hi.hpp"

#define GE_MODELLER_IMPLEMENTATION
#include "render/window.hpp"

int main() {
    ge::ConfigBinary cfg{};
    if (!ge::ensure_config_binary(cfg)) io::exit_process(-2);

    io::unique_ptr<Window> win = io::make_unique<Window>(cfg);
    if (!win) io::exit_process(-3);

    const int load_code = win->LoadResources();
    if (load_code != 0) io::exit_process(-10 - load_code);

    while (!win->ShouldQuit() && win->PollEvents())
        win->Render();

    io::exit_process(0);
}
