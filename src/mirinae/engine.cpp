#include "mirinae/engine.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>

#include <spdlog/spdlog.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <daltools/util.h>

#include "mirinae/render/glprimitive.hpp"
#include "mirinae/util/filesys.hpp"


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


    class GlfwWindow {

    public:
        GlfwWindow(void* userdata) {
            this->window = glfwCreateWindow(640, 480, "My Title", nullptr, nullptr);
            glfwMakeContextCurrent(this->window);
            glfwSetFramebufferSizeCallback(this->window, GlfwWindow::window_size_callback);
            glfwSetWindowUserPointer(this->window, userdata);

            int version = gladLoadGL(glfwGetProcAddress);
            if (0 == version)
                throw std::runtime_error{ "Failed to initialize GLAD" };
            spdlog::info("OpenGL version: {}.{}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
        }

        ~GlfwWindow() {
            if (nullptr != this->window) {
                glfwDestroyWindow(this->window);
                this->window = nullptr;
            }
        }

        void swap_buffer() {
            glfwSwapBuffers(this->window);
        }

        bool is_ongoing() const {
            return !glfwWindowShouldClose(this->window);
        }

    private:
        static void window_size_callback(GLFWwindow* window, int width, int height) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;

            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);
            engine->on_window_resize(width, height);
        }

        GLFWwindow* window = nullptr;

    };


    class QuatCamera {

    public:
        void rotate(const float angle, const glm::vec3& axis) {
            this->quat = this->rotate_quat(this->quat, angle, axis);
        }

        glm::mat4 make_view_mat() const {
            const auto r = glm::toMat4(this->quat);
            const auto t = glm::translate(r, -this->pos);
            return t;
        }

        glm::vec3 pos{};

    private:
        glm::quat rotate_quat(const glm::quat& q, const float angle, const glm::vec3& axis) {
            return glm::normalize(glm::angleAxis(angle, axis) * q);
        }

        glm::quat quat{};

    };


    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw()
            : window(this)
        {
            float vertices[] = {
                -1, -0.5 * std::sqrt(3.0), 0,
                 1, -0.5 * std::sqrt(3.0), 0,
                 0,  0.5 * std::sqrt(3.0), 0
            };
            vbo.init(vertices, sizeof(vertices));
            vao.init(vbo);

            const auto res_dir_path = mirinae::find_resources_folder();
            const auto vertex_src = mirinae::read_file<std::string>((*res_dir_path / "shader/tutorial.vert").u8string().c_str());
            const auto fragment_src = mirinae::read_file<std::string>((*res_dir_path / "shader/tutorial.frag").u8string().c_str());
            mirinae::ShaderUnit vertex_shader{ mirinae::ShaderUnit::Type::vertex, vertex_src->c_str() };
            mirinae::ShaderUnit fragment_shader{ mirinae::ShaderUnit::Type::fragment, fragment_src->c_str() };
            this->program.init(vertex_shader, fragment_shader);

            this->camera.pos.z = 5;

            {
                int width, height, nrChannels;
                const auto data = stbi_load((*res_dir_path / "texture/missing_texture.png").u8string().c_str(), &width, &height, &nrChannels, 0);
                this->texture.init(width, height, data);
            }

            this->on_window_resize(640, 480);
        }

        void do_frame() override {
            // Update scene

            const auto delta_time = this->timer.check_get_elapsed();
            this->camera.rotate(delta_time, glm::vec3{0, 0, 1});

            // Render

            const auto sec = dal::get_cur_sec();
            glClearColor(std::sin(sec * 0.3), std::sin(sec * 1.7), std::sin(sec * 3.4), 1);
            glClear(GL_COLOR_BUFFER_BIT);

            this->program.use();

            const auto mat = this->proj_mat * this->camera.make_view_mat();
            glUniformMatrix4fv(this->program.get_uniform_loc("u_proj_mat"), 1, GL_FALSE, glm::value_ptr(mat));

            this->vao.use();
            glDrawArrays(GL_TRIANGLES, 0, 3);
            mirinae::log_gl_error("do_frame");

            this->window.swap_buffer();
            glfwPollEvents();
        }

        bool is_ongoing() override {
            return this->window.is_ongoing();
        }

        void on_window_resize(unsigned width, unsigned height) override {
            glViewport(0, 0, width, height);
            this->proj_mat = glm::perspective<float>(90, static_cast<double>(width) / static_cast<double>(height), 0.1, 100);
            spdlog::info("Window resize: {}x{}", width, height);
        }

    private:
        GlfwWindow window;
        dal::Timer timer;
        mirinae::BufferObject vbo;
        mirinae::VertexArrayObject vao;
        mirinae::ShaderProgram program;
        mirinae::Texture texture;
        glm::mat4 proj_mat;
        QuatCamera camera;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
