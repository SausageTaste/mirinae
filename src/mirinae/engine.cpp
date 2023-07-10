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
            glfwSetWindowUserPointer(this->window, userdata);

            glfwSetFramebufferSizeCallback(this->window, GlfwWindow::callback_fbuf_size);
            glfwSetKeyCallback(this->window, GlfwWindow::callback_key);

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
        static void callback_fbuf_size(GLFWwindow* window, int width, int height) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);
            engine->on_window_resize(width, height);
        }

        static void callback_key(GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);

            mirinae::key::Event e;
            switch (action) {
                case GLFW_RELEASE:
                    e.action_type = mirinae::key::ActionType::up;
                    break;
                case GLFW_PRESS:
                case GLFW_REPEAT:
                default:
                    e.action_type = mirinae::key::ActionType::down;
                    break;
            }

            e.key = map_key_code(key);
            engine->notify_key_event(e);
        }

        static mirinae::key::KeyCode map_key_code(const int glfw_key) {
            if (GLFW_KEY_A <= glfw_key && glfw_key <= GLFW_KEY_Z) {
                auto index = glfw_key - GLFW_KEY_A + int(mirinae::key::KeyCode::a);
                return mirinae::key::KeyCode(index);
            }
            else if (GLFW_KEY_0 <= glfw_key && glfw_key <= GLFW_KEY_9) {
                auto index = glfw_key - GLFW_KEY_0 + int(mirinae::key::KeyCode::n0);
                return mirinae::key::KeyCode(index);
            }
            else {
                static const std::unordered_map<uint32_t, mirinae::key::KeyCode> map{
                    {GLFW_KEY_GRAVE_ACCENT, mirinae::key::KeyCode::backquote},
                    {GLFW_KEY_MINUS, mirinae::key::KeyCode::minus},
                    {GLFW_KEY_EQUAL, mirinae::key::KeyCode::equal},
                    {GLFW_KEY_LEFT_BRACKET, mirinae::key::KeyCode::lbracket},
                    {GLFW_KEY_RIGHT_BRACKET, mirinae::key::KeyCode::rbracket},
                    {GLFW_KEY_BACKSLASH, mirinae::key::KeyCode::backslash},
                    {GLFW_KEY_SEMICOLON, mirinae::key::KeyCode::semicolon},
                    {GLFW_KEY_APOSTROPHE, mirinae::key::KeyCode::quote},
                    {GLFW_KEY_COMMA, mirinae::key::KeyCode::comma},
                    {GLFW_KEY_PERIOD, mirinae::key::KeyCode::period},
                    {GLFW_KEY_SLASH, mirinae::key::KeyCode::slash},

                    {GLFW_KEY_SPACE, mirinae::key::KeyCode::space},
                    {GLFW_KEY_ENTER, mirinae::key::KeyCode::enter},
                    {GLFW_KEY_BACKSPACE, mirinae::key::KeyCode::backspace},
                    {GLFW_KEY_TAB, mirinae::key::KeyCode::tab},

                    {GLFW_KEY_ESCAPE, mirinae::key::KeyCode::escape},
                    {GLFW_KEY_LEFT_SHIFT, mirinae::key::KeyCode::lshfit},
                    {GLFW_KEY_RIGHT_SHIFT, mirinae::key::KeyCode::rshfit},
                    {GLFW_KEY_LEFT_CONTROL, mirinae::key::KeyCode::lctrl},
                    {GLFW_KEY_RIGHT_CONTROL, mirinae::key::KeyCode::rctrl},
                    {GLFW_KEY_LEFT_ALT, mirinae::key::KeyCode::lalt},
                    {GLFW_KEY_RIGHT_ALT, mirinae::key::KeyCode::ralt},
                    {GLFW_KEY_UP, mirinae::key::KeyCode::up},
                    {GLFW_KEY_DOWN, mirinae::key::KeyCode::down},
                    {GLFW_KEY_LEFT, mirinae::key::KeyCode::left},
                    {GLFW_KEY_RIGHT, mirinae::key::KeyCode::right},
                };

                auto res = map.find(glfw_key);
                if (res == map.end()) {
                    return mirinae::key::KeyCode::eoe;
                }
                else {
                    return res->second;
                }
            }
        }

        GLFWwindow* window = nullptr;

    };


    class QuatCamera {

    public:
        void rotate(const float angle, const glm::vec3& axis) {
            this->quat = this->rotate_quat(this->quat, angle, axis);
        }

        glm::vec3 make_view_direc() const {
            const auto r = glm::toMat4(this->quat);
            const auto v = r * glm::vec4{ 0, 0, -1, 0 };
            return glm::normalize(glm::vec3{ v });
        }

        glm::mat4 make_view_mat() const {
            const auto r = glm::inverse(glm::toMat4(this->quat));
            const auto t = glm::translate(r, -this->pos);
            return t;
        }

        void move_along_view_direc(const glm::vec3& v) {
            const auto new_vec = glm::toMat4(this->quat) * glm::vec4{ v, 0 };
            this->pos += glm::vec3{ new_vec };
        }

        void rotate_view_up(float radians) {
            const auto right = glm::toMat4(this->quat) * glm::vec4{ 1, 0, 0, 0 };
            this->rotate(radians, glm::vec3(right));
        }

        void rotate_view_left(float radians) {
            this->rotate(radians, glm::vec3{ 0, 1, 0 });
        }

        void rotate_tilt(float radians) {
            this->rotate(radians, this->make_view_direc());
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
            const auto res_dir_path = mirinae::find_resources_folder();
            const auto vertex_src = mirinae::load_file<std::string>((*res_dir_path / "shader/tutorial.vert").u8string().c_str());
            const auto fragment_src = mirinae::load_file<std::string>((*res_dir_path / "shader/tutorial.frag").u8string().c_str());
            mirinae::ShaderUnit vertex_shader{ mirinae::ShaderUnit::Type::vertex, vertex_src->c_str() };
            mirinae::ShaderUnit fragment_shader{ mirinae::ShaderUnit::Type::fragment, fragment_src->c_str() };
            this->program.init(vertex_shader, fragment_shader);

            this->camera.pos.y = 3;

            // Missing texture
            {
                auto path = *res_dir_path / "texture/missing_texture.png";
                auto image = mirinae::load_image(path.u8string().c_str());
                if (!image.is_ready()) {
                    spdlog::error("Failed to load image: {}", path.u8string());
                    throw std::runtime_error{""};
                }
                this->texture_error.init(image.width(), image.height(), image.channels(), image.data());
            }

            // Grass texture
            {
                const auto path = *res_dir_path / "texture/grass1.tga";
                auto image = mirinae::load_image(path.u8string().c_str());
                if (!image.is_ready()) {
                    spdlog::error("Failed to load image: {}", path.u8string());
                    throw std::runtime_error{""};
                }
                this->texture_grass.init(image.width(), image.height(), image.channels(), image.data());
            }

            // Simple quad mesh
            {
                std::vector<mirinae::VertexStatic> vertices{
                    mirinae::VertexStatic{ 5,  5, 0,  1, 0, 0,  1, 1}, // top right
                    mirinae::VertexStatic{ 5, -5, 0,  1, 0, 0,  1, 0}, // bottom right
                    mirinae::VertexStatic{-5, -5, 0,  1, 0, 1,  0, 0}, // bottom left
                    mirinae::VertexStatic{-5,  5, 0,  1, 0, 0,  0, 1}  // top left
                };
                std::vector<unsigned int> indices{
                    0, 3, 1, // first triangle
                    1, 3, 2  // second triangle
                };
                this->mesh0.init(vertices, indices);
            }

            // Terrain mesh
            {
                const auto path = *res_dir_path / "texture" / "iceland_heightmap.png";
                auto image = mirinae::load_image(path.u8string().c_str());
                if (!image.is_ready()) {
                    spdlog::error("Failed to load image: {}", path.u8string());
                    throw std::runtime_error{""};
                }

                const auto width = static_cast<float>(image.width());
                const auto height = static_cast<float>(image.height());

                std::vector<mirinae::VertexStatic> vertices;
                for (size_t i = 0; i < image.height(); i++) {
                    for (size_t j = 0; j < image.width(); j++) {
                        const auto texel = image.get_texel_at_clamp(j, i);
                        unsigned char y = texel[0];

                        // vertex
                        const glm::vec3 pos{
                            -double(height) / 2.f + i,
                            double(y) * 64.f / 255.f,
                            -double(width) / 2.f + j
                        };

                        auto& v = vertices.emplace_back();
                        v.pos() = pos * 0.05f;
                        v.uv().x = double(j) / width * 100.f;
                        v.uv().y = double(i) / height * 100.f;

                        const auto hl = image.get_texel_at_clamp(j - 1, i)[0] / 255.f;
                        const auto hr = image.get_texel_at_clamp(j + 1, i)[0] / 255.f;
                        const auto hd = image.get_texel_at_clamp(j, i - 1)[0] / 255.f;
                        const auto hu = image.get_texel_at_clamp(j, i + 1)[0] / 255.f;

                        v.normal().x = hl - hr;
                        v.normal().y = hd - hu;
                        v.normal().z = 2 * 0.05f;;
                        v.normal() = glm::normalize(v.normal());

                        continue;
                    }
                }

                std::vector<unsigned int> indices;
                for (size_t y = 0; y < image.height() - 1; y++) {
                    for (size_t x = 0; x < image.width() - 1; x++) {
                        indices.push_back(y * width + x);
                        indices.push_back((y + 1) * width + x + 1);
                        indices.push_back((y + 1) * width + x);

                        indices.push_back(y * width + x);
                        indices.push_back(y * width + x + 1);
                        indices.push_back((y + 1) * width + x + 1);
                    }
                }

                this->mesh1.init(vertices, indices);
            }

            glPatchParameteri(GL_PATCH_VERTICES, 4);

            // Tessellation mesh
            {
                const auto path = *res_dir_path / "texture" / "iceland_heightmap.png";
                auto image = mirinae::load_image(path.u8string().c_str());
                if (!image.is_ready()) {
                    spdlog::error("Failed to load image: {}", path.u8string());
                    throw std::runtime_error{""};
                }

                texture_height.init(image.width(), image.height(), image.channels(), image.data());

                const auto width = static_cast<float>(image.width());
                const auto height = static_cast<float>(image.height());

                std::vector<mirinae::VertexStatic> vertices;
                for (unsigned i = 0; i <= rez; ++i) {
                    for (unsigned j = 0; j <= rez; ++j) {
                        auto v = &vertices.emplace_back();
                        v->pos().x = (-width/2.0f + width*i/(float)rez);
                        v->pos().y = (0.0f);
                        v->pos().z = (-height/2.0f + height*j/(float)rez);
                        v->uv().x = (i / (float)rez);
                        v->uv().y = (j / (float)rez);

                        v = &vertices.emplace_back();
                        v->pos().x = (-width/2.0f + width*(i+1)/(float)rez); // v.x
                        v->pos().y = (0.0f); // v.y
                        v->pos().z = (-height/2.0f + height*j/(float)rez); // v.z
                        v->uv().x = ((i+1) / (float)rez); // u
                        v->uv().y = (j / (float)rez); // v

                        v = &vertices.emplace_back();
                        v->pos().x = (-width/2.0f + width*i/(float)rez); // v.x
                        v->pos().y = (0.0f); // v.y
                        v->pos().z = (-height/2.0f + height*(j+1)/(float)rez); // v.z
                        v->uv().x = (i / (float)rez); // u
                        v->uv().y = ((j+1) / (float)rez); // v

                        v = &vertices.emplace_back();
                        v->pos().x = (-width/2.0f + width*(i+1)/(float)rez); // v.x
                        v->pos().y = (0.0f); // v.y
                        v->pos().z = (-height/2.0f + height*(j+1)/(float)rez); // v.z
                        v->uv().x = ((i+1) / (float)rez); // u
                        v->uv().y = ((j+1) / (float)rez); // v
                    }
                }

                glGenVertexArrays(1, &terrainVAO);
                glBindVertexArray(terrainVAO);

                glGenBuffers(1, &terrainVBO);
                glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
                glBufferData(GL_ARRAY_BUFFER, mirinae::VertexStatic::data_size() * vertices.size(), &vertices[0], GL_STATIC_DRAW);

                // Pos attribute
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, mirinae::VertexStatic::data_size(), (void*)0);
                glEnableVertexAttribArray(0);
                // Normal attribute
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, mirinae::VertexStatic::data_size(), (void*)(3 * sizeof(float)));
                glEnableVertexAttribArray(1);
                // UV attribute
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, mirinae::VertexStatic::data_size(), (void*)(6 * sizeof(float)));
                glEnableVertexAttribArray(2);

                glPatchParameteri(GL_PATCH_VERTICES, NUM_PATCH_PTS);
            }

            {
                const auto vertex_src = mirinae::load_file<std::string>((*res_dir_path / "shader/tessell.vert").u8string().c_str());
                const auto tess_ctrl = mirinae::load_file<std::string>((*res_dir_path / "shader/tessell.tesc").u8string().c_str());
                const auto tess_eval = mirinae::load_file<std::string>((*res_dir_path / "shader/tessell.tese").u8string().c_str());
                const auto fragment_src = mirinae::load_file<std::string>((*res_dir_path / "shader/tessell.frag").u8string().c_str());

                mirinae::ShaderUnit vertex_shader{ mirinae::ShaderUnit::Type::vertex, vertex_src->c_str() };
                mirinae::ShaderUnit tess_ctrl_shader{ mirinae::ShaderUnit::Type::tessell_ctrl, tess_ctrl->c_str() };
                mirinae::ShaderUnit tess_eval_shader{ mirinae::ShaderUnit::Type::tessell_eval, tess_eval->c_str() };
                mirinae::ShaderUnit fragment_shader{ mirinae::ShaderUnit::Type::fragment, fragment_src->c_str() };
                this->program_tess.init(vertex_shader, tess_ctrl_shader, tess_eval_shader, fragment_shader);
            }

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CW);

            this->on_window_resize(640, 480);
        }

        void do_frame() override {
            const auto delta_time = this->timer.check_get_elapsed();

            // User control

            glm::vec3 move_direc{};
            if (key_anal[mirinae::key::KeyCode::w].pressed)
                move_direc.z -= 1;
            if (key_anal[mirinae::key::KeyCode::s].pressed)
                move_direc.z += 1;
            if (key_anal[mirinae::key::KeyCode::a].pressed)
                move_direc.x -= 1;
            if (key_anal[mirinae::key::KeyCode::d].pressed)
                move_direc.x += 1;

            const float move_speed = delta_time * (key_anal[mirinae::key::KeyCode::lshfit].pressed ? 10 : 1);
            this->camera.move_along_view_direc(move_direc * move_speed);

            if (key_anal[mirinae::key::KeyCode::space].pressed)
                this->camera.pos.y += move_speed;
            if (key_anal[mirinae::key::KeyCode::lctrl].pressed)
                this->camera.pos.y -= move_speed;

            if (key_anal[mirinae::key::KeyCode::left].pressed)
                this->camera.rotate_view_left(delta_time);
            if (key_anal[mirinae::key::KeyCode::right].pressed)
                this->camera.rotate_view_left(-delta_time);
            if (key_anal[mirinae::key::KeyCode::up].pressed)
                this->camera.rotate_view_up(delta_time);
            if (key_anal[mirinae::key::KeyCode::down].pressed)
                this->camera.rotate_view_up(-delta_time);
            if (key_anal[mirinae::key::KeyCode::q].pressed)
                this->camera.rotate_tilt(-delta_time);
            if (key_anal[mirinae::key::KeyCode::e].pressed)
                this->camera.rotate_tilt(delta_time);

            // Update scene

            // Render

            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const glm::mat4 identity_mat{1};
            const auto view_mat = this->camera.make_view_mat();
            const auto mat = this->proj_mat * this->camera.make_view_mat();

            this->program.use();
            glUniformMatrix4fv(this->program.get_uniform_loc("u_proj_mat"), 1, GL_FALSE, glm::value_ptr(mat));
            glUniform1i(this->program.get_uniform_loc("texture1"), 0);
            glUniform1i(this->program.get_uniform_loc("texture2"), 1);
            this->texture_error.use(0);
            this->texture_grass.use(1);
            this->mesh0.draw();
            //this->mesh1.draw();
            mirinae::log_gl_error("do_frame, mesh");

            this->program_tess.use();
            glUniform1i(this->program_tess.get_uniform_loc("heightMap"), 0);
            this->texture_height.use(0);
            glUniformMatrix4fv(this->program_tess.get_uniform_loc("model"), 1, GL_FALSE, glm::value_ptr(identity_mat));
            glUniformMatrix4fv(this->program_tess.get_uniform_loc("view"), 1, GL_FALSE, glm::value_ptr(view_mat));
            glUniformMatrix4fv(this->program_tess.get_uniform_loc("projection"), 1, GL_FALSE, glm::value_ptr(this->proj_mat));
            glBindVertexArray(terrainVAO);
            glDrawArrays(GL_PATCHES, 0, NUM_PATCH_PTS*rez*rez);
            mirinae::log_gl_error("do_frame, compute");

            this->window.swap_buffer();
            glfwPollEvents();
        }

        bool is_ongoing() override {
            return this->window.is_ongoing();
        }

        void on_window_resize(unsigned width, unsigned height) override {
            glViewport(0, 0, width, height);
            this->proj_mat = glm::perspective<float>(90, static_cast<double>(width) / static_cast<double>(height), 0.1, 1000);
            spdlog::info("Window resize: {}x{}", width, height);
        }

        void notify_key_event(const mirinae::key::Event& e) override {
            this->key_anal.notify(e);
        }

    private:
        const unsigned int NUM_PATCH_PTS = 4;
        const unsigned rez = 20;

        GlfwWindow window;
        dal::Timer timer;
        mirinae::key::EventAnalyzer key_anal;
        mirinae::MeshStatic mesh0;
        mirinae::MeshStatic mesh1;
        mirinae::MeshStatic mesh2;
        mirinae::ShaderProgram program;
        mirinae::ShaderProgram program_tess;
        mirinae::Texture texture_error;
        mirinae::Texture texture_grass;
        mirinae::Texture texture_height;
        glm::mat4 proj_mat;
        QuatCamera camera;
        unsigned terrainVAO, terrainVBO;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
