#pragma once

#include <stdexcept>

#include <spdlog/spdlog.h>
#include <glad/gl.h>


namespace mirinae {

    void log_gl_error(const char* label = "") {
        const auto err = glGetError();
        if (GL_NO_ERROR != err) {
            spdlog::warn("OpenGL error: 0x{:x} ({})", err, label);
        }
    }


    class BufferObject {

    public:
        ~BufferObject() {
            this->destroy();
        }

        void init(const void* data, size_t size) {
            glGenBuffers(1, &this->handle);
            glBindBuffer(GL_ARRAY_BUFFER, this->handle);
            glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
            log_gl_error();
        }

        void destroy() {
            if (0 != this->handle) {
                glDeleteBuffers(1, &this->handle);
                this->handle = 0;
            }
        }

        bool is_ready() const {
            return this->handle != 0;
        }

        GLuint get_handle() const { return this->handle; }

    private:
        GLuint handle = 0;

    };


    class VertexArrayObject {

    public:
        ~VertexArrayObject() {
            this->destroy();
        }

        void init(BufferObject& vbo) {
            glGenVertexArrays(1, &this->handle);
            glBindVertexArray(this->handle);

            glBindBuffer(GL_ARRAY_BUFFER, vbo.get_handle());

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
        }

        void destroy() {
            if (0 != this->handle) {
                glDeleteVertexArrays(1, &this->handle);
                this->handle = 0;
            }
        }

        void use() {
            glBindVertexArray(this->handle);
        }

    private:
        GLuint handle = 0;

    };


    class ShaderUnit {

    public:
        enum class Type {
            vertex,
            fragment,
        };

    public:
        ShaderUnit(Type type, const char* source) {
            this->handle = glCreateShader(this->interpret_shader_type(type));
            if (0 == this->handle) {
                log_gl_error();
                return;
            }

            glShaderSource(this->handle, 1, &source, nullptr);
            glCompileShader(this->handle);

            if (!this->is_success()) {
                char msg[512];
                glGetShaderInfoLog(this->handle, 512, nullptr, msg);
                spdlog::error("Shader compilation failed...\n{}", msg);
            }
        }

        ~ShaderUnit() {
            this->destroy();
        }

        void destroy() {
            if (0 != this->handle) {
                glDeleteShader(this->handle);
                this->handle = 0;
            }
        }

        bool is_ready() const {
            return this->is_success();
        }

        GLuint get_handle() const { return this->handle; }

    private:
        bool is_success() const {
            if (0 == this->handle)
                return false;

            int success;
            glGetShaderiv(this->handle, GL_COMPILE_STATUS, &success);
            return 0 != success;
        }

        static GLenum interpret_shader_type(Type type) {
            switch (type) {
                case Type::vertex:
                    return GL_VERTEX_SHADER;
                case Type::fragment:
                    return GL_FRAGMENT_SHADER;
                default:
                    spdlog::error("Unknown ShaderUnit::Type value: {}", static_cast<int>(type));
                    throw std::runtime_error{""};
            }
        }

        GLuint handle = 0;

    };


    class ShaderProgram {

    public:
        ~ShaderProgram() {
            this->destroy();
        }

        void init(const ShaderUnit& vertex, const ShaderUnit& fragment) {
            this->handle = glCreateProgram();
            glAttachShader(this->handle, vertex.get_handle());
            glAttachShader(this->handle, fragment.get_handle());
            glLinkProgram(this->handle);

            log_gl_error("Shader program linking");

            if (!this->is_success()) {
                char msg[512];
                glGetProgramInfoLog(this->handle, 512, NULL, msg);
                spdlog::error("Shader linking failed...\n{}", msg);
            }
        }

        void destroy() {
            if (0 != this->handle) {
                glDeleteProgram(this->handle);
                this->handle = 0;
            }
        }

        bool is_ready() const {
            if (0 == this->handle)
                return false;

            return true;
        }

        void use() {
            if (this->is_ready())
                glUseProgram(this->handle);
        }

    private:
        bool is_success() const {
            if (0 == this->handle)
                return false;

            int success;
            glGetProgramiv(this->handle, GL_LINK_STATUS, &success);
            return 0 != success;
        }

        GLuint handle = 0;

    };

}
