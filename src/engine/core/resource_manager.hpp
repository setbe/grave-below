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

    private:
        // Do not check for errors, receive error info on program linkage stage
        template<io::usize N>
        static inline void read_and_compile_shader(io::StackOut<N> stack_path,
                                                   io::char_view filename,
                                                   io::u32& shader_id,
                                                   gl::ShaderType shader_type,
                                                   io::string& source) noexcept {
            stack_path.reset();
            stack_path << PATH_RESOURCES << PATH_SHADERS << filename;
            {
                fs::File f{ stack_path.view(), io::OpenMode::Read};
                f.read_all(source);
                gl::Shader::compile_shader(shader_id, shader_type, source.c_str());
            }
        }
    }; // struct ResourceManager

    bool ResourceManager::shader_from(io::char_view frag_filename,
                                      io::char_view vert_filename,
                                      gl::Shader& shader) noexcept {
        io::StackOut<980> stack_path{};
        {
            const io::usize max_filename_size =
                (frag_filename.size() > vert_filename.size()) ?
                frag_filename.size() : vert_filename.size();

            if (stack_path.value_type() <= PATH_RESOURCES.size() + max_filename_size) {
                log(LogFrom::Resource, LogWhat::StackOutLimit);
                return false;
            }
        }
        io::u32 frag_id{};
        io::u32 vert_id{};
        io::u32 prog_id{};

        {
            io::string source{};
            read_and_compile_shader(stack_path, vert_filename, vert_id, gl::ShaderType::VertexShader, source);
            read_and_compile_shader(stack_path, frag_filename, frag_id, gl::ShaderType::FragmentShader, source);
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