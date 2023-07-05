#include "mirinae/engine.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>

#include <spdlog/spdlog.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "mirinae/render/glprimitive.hpp"


namespace {

    class GlfwRaii {

    public:
        GlfwRaii() {
            glfwInit();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }

        ~GlfwRaii() {
            glfwTerminate();
        }

    } g_glfw_raii;


    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw()
            : created_time(std::chrono::steady_clock::now())
        {
            this->window = glfwCreateWindow(640, 480, "My Title", nullptr, nullptr);
            glfwMakeContextCurrent(this->window);

            int version = gladLoadGL(glfwGetProcAddress);
            if (0 == version)
                throw std::runtime_error{ "Failed to initialize GLAD" };
            spdlog::info("OpenGL version: {}.{}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

            float vertices[] = {
                -0.5f, -0.5f, 0.0f,
                0.5f, -0.5f, 0.0f,
                0.0f,  0.5f, 0.0f
            };
            vbo.init(vertices, sizeof(vertices));
            vao.init(vbo);

            const char* vertex_src = "#version 450 core\n"
                "layout (location = 0) in vec3 aPos;\n"
                "void main()\n"
                "{\n"
                "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                "}\0";
            const char* fragment_src = "#version 450 core\n"
                "out vec4 FragColor;\n"
                "void main()\n"
                "{\n"
                "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
                "}\0";
            mirinae::ShaderUnit vertex_shader{ mirinae::ShaderUnit::Type::vertex, vertex_src };
            mirinae::ShaderUnit fragment_shader{ mirinae::ShaderUnit::Type::fragment, fragment_src };
            this->program.init(vertex_shader, fragment_shader);
        }

        ~EngineGlfw() {
            this->destroy();
        }

        void destroy() {
            if (nullptr != this->window) {
                glfwDestroyWindow(this->window);
                this->window = nullptr;
            }
        }

        void do_frame() override {
            const auto dur = std::chrono::steady_clock::now() - this->created_time;
            const auto sec = static_cast<float>(dur.count()) / 1000000000.f;

            glClearColor(std::sin(sec * 0.3f), std::sin(sec * 1.7f), std::sin(sec * 3.4f), 1);
            glClear(GL_COLOR_BUFFER_BIT);

            this->program.use();
            this->vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 3);
            mirinae::log_gl_error("do_frame");

            glfwSwapBuffers(this->window);
            glfwPollEvents();
        }

        bool is_ongoing() override {
            return !glfwWindowShouldClose(this->window);
        }

    private:
        GLFWwindow* window = nullptr;
        std::chrono::steady_clock::time_point created_time;
        mirinae::BufferObject vbo;
        mirinae::VertexArrayObject vao;
        mirinae::ShaderProgram program;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
