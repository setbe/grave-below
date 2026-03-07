#pragma once

#include "../../../3rd_party/hi/hi/io.hpp"
#include "../../../3rd_party/hi/hi/gl_loader.hpp"

#include "config.hpp"
#include "logger.hpp"

namespace ge {
    struct ResourceManager {
        static inline bool shader_from(io::char_view frag_filename,
                                       io::char_view vert_filename,
                                       gl::Shader& shader) noexcept;
    }; // struct ResourceManager

    bool ResourceManager::shader_from(io::char_view frag_filename, io::char_view vert_filename, gl::Shader& shader) noexcept {
        io::StackOut<980> stack_path{};
        {
            const io::usize max_filename_size =
                (frag_filename.size() > vert_filename.size()) ?
                frag_filename.size() : vert_filename.size();

            if (stack_path.storage.view().size() <= RESOURCE_PATH.size() + max_filename_size) {
                log(LogFrom::Resource, LogWhat::StackOutLimit);
                return false;
            }
        }
        io::string source{};
        io::u32 frag_id{};
        io::u32 vert_id{};
        io::u32 prog_id{};

        // --- vertex ---
       stack_path << RESOURCE_PATH << vert_filename;
        {
            fs::File f{ stack_path.view(), io::OpenMode::Read};
            f.read_all(source);
            if (!gl::Shader::compile_shader(vert_id, gl::ShaderType::VertexShader, source.c_str())) {
                log(LogFrom::Resource, LogWhat::CompileShader);
                return false;
            }
        }

        // --- fragment ---
        stack_path << RESOURCE_PATH << frag_filename;
        {
            fs::File f{ stack_path.view(), io::OpenMode::Read};
            f.read_all(source);
            if (!gl::Shader::compile_shader(frag_id, gl::ShaderType::FragmentShader, source.c_str())) {
                gl::DeleteShader(vert_id);
                log(LogFrom::Resource, LogWhat::CompileShader);
                return false;
            }
        }
        const bool ok_prog = gl::Shader::link_program(prog_id, vert_id, frag_id);
        gl::DeleteShader(vert_id);
        gl::DeleteShader(frag_id);
        if (!ok_prog) {
            log(LogFrom::Resource, LogWhat::LinkShaderProgram);
            return false;
        }
        shader.~Shader();
        new (&shader) gl::Shader{ prog_id };
        return true;
    } // shader_from
} // namespage ge