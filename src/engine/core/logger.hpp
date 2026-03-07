#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"

#include "config.hpp"

namespace ge {
    enum class LogFrom {
        None,
        Resource,
    };

    enum class LogWhat {
        None,
        StackOutLimit,
        CompileShader,
        LinkShaderProgram,
    };

    static io::char_view log_from_str(LogFrom);
    static io::char_view log_what_str(LogWhat);

    static void log(io::char_view view) noexcept {
        fs::File f{ LOG_FILENAME, io::OpenMode::Append };
        f.write_line(view);
    }

    static void log(LogFrom from, LogWhat what) noexcept {
        io::StackOut<220> ss{};
        ss << log_from_str(from)
           << ": "
           << ((what != LogWhat::None) ? log_what_str(what) : "")
           << '\n';
        log(ss.view());
    }

    static io::char_view log_from_str(LogFrom from) {
        using F = LogFrom;
        switch (from)
        {
        case F::None: return "none";
        case F::Resource: return "Resource";
        default: return "unknown";
        }
    } // log_from_str


    static io::char_view log_what_str(LogWhat what) {
        using W = LogWhat;
        switch (what)
        {
        case W::None: return "none";
        case W::StackOutLimit: return "StackOut is limited";
        case W::CompileShader: return "Compile shader";
        case W::LinkShaderProgram: return "Link shader program";
        default: return "unknown";
        }
    } // log_what_str
}