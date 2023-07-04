#include "mirinae/engine.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>

#include <spdlog/spdlog.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

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

            glfwSwapBuffers(this->window);
            glfwPollEvents();
        }

        bool is_ongoing() override {
            return !glfwWindowShouldClose(this->window);
        }

    private:
        GLFWwindow* window = nullptr;
        std::chrono::steady_clock::time_point created_time;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
