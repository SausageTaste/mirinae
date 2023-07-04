#include "mirinae/engine.hpp"

#include <cassert>
#include <stdexcept>

#include <GLFW/glfw3.h>

namespace {

    class GlfwRaii {
    public:
        GlfwRaii() { glfwInit(); }
        ~GlfwRaii() { glfwTerminate(); }
    } g_glfw_raii;


    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw() {
            this->window = glfwCreateWindow(640, 480, "My Title", nullptr, nullptr);
            assert(nullptr != this->window);
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
            glfwSwapBuffers(this->window);
            glfwPollEvents();
        }

        bool is_ongoing() override {
            return !glfwWindowShouldClose(this->window);
        }

    private:
        GLFWwindow* window = nullptr;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
