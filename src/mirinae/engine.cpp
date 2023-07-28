#include "mirinae/engine.hpp"

#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <mirinae/render/pipeline.hpp>
#include <mirinae/render/vkmajorplayers.hpp>


namespace {

    auto get_glfw_extensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        return std::vector<std::string>{ glfwExtensions, glfwExtensions + glfwExtensionCount };
    }


    class GlfwRaii {

    public:
        GlfwRaii() {
            glfwInit();
            spdlog::set_level(spdlog::level::level_enum::trace);
        }

        ~GlfwRaii() {
            glfwTerminate();
        }

    } g_glfw_raii;


    class GlfwWindow {

    public:
        GlfwWindow(void* userdata) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
            this->window = glfwCreateWindow(800, 450, "Mirinapp", nullptr, nullptr);

            glfwSetFramebufferSizeCallback(this->window, GlfwWindow::callback_fbuf_size);
            glfwSetKeyCallback(this->window, GlfwWindow::callback_key);
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

        VkSurfaceKHR create_surface(const mirinae::VulkanInstance& instance) {
            VkSurfaceKHR surface = nullptr;
            if (VK_SUCCESS != glfwCreateWindowSurface(instance.get(), window, nullptr, &surface)) {
                spdlog::error("Failed to create window surface");
                return nullptr;
            }
            return surface;
        }

        std::pair<int, int> get_fbuf_size() const {
            std::pair<int, int> output{ 0, 0 };
            if (nullptr == this->window)
                return output;

            glfwGetFramebufferSize(window, &output.first, &output.second);
            return output;
        }

    private:
        static void callback_fbuf_size(GLFWwindow* window, int width, int height) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);
            engine->notify_window_resize(width, height);
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


    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw()
            : window_(this)
        {
            mirinae::InstanceFactory instance_factory;
            instance_factory.enable_validation_layer();
            instance_factory.ext_layers_.add_validation();
            {
                const auto glfwExtensions = ::get_glfw_extensions();
                instance_factory.ext_layers_.extensions_.insert(instance_factory.ext_layers_.extensions_.end(), glfwExtensions.begin(), glfwExtensions.end());
            }

            instance_.init(instance_factory);
            surface_ = window_.create_surface(instance_);
            phys_device_.set(instance_.select_phys_device(surface_), surface_);
            spdlog::info("Physical device selected: {}", phys_device_.name());

            std::vector<std::string> device_extensions;
            device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            if (phys_device_.count_unsupported_extensions(device_extensions))
                throw std::runtime_error{ "Some extensions are not supported" };
            logi_device_.init(phys_device_, device_extensions);

            mirinae::SwapChainSupportDetails swapchain_details;
            swapchain_details.init(surface_, phys_device_.get());
            if (!swapchain_details.is_complete()) {
                throw std::runtime_error{ "The swapchain is not complete" };
            }

            const auto [fbuf_width, fbuf_height] = window_.get_fbuf_size();
            swapchain_.init(fbuf_width, fbuf_height, surface_, phys_device_, logi_device_);

            pipeline_layout_ = mirinae::create_unorthodox_pipeline(swapchain_.extent(), logi_device_.get());
            renderpass_.init(swapchain_.format(), logi_device_);

            return;
        }

        ~EngineGlfw() {
            swapchain_.destroy(logi_device_);
            vkDestroySurfaceKHR(instance_.get(), surface_, nullptr); surface_ = nullptr;
            logi_device_.destroy();
        }

        void do_frame() override {
            window_.swap_buffer();
            glfwPollEvents();
        }

        bool is_ongoing() override {
            return window_.is_ongoing();
        }

        void notify_window_resize(unsigned width, unsigned height) {
            spdlog::info("Window resized: {} x {}", width, height);
        }

        void notify_key_event(const mirinae::key::Event& e) {

        }

    private:
        GlfwWindow window_;
        mirinae::VulkanInstance instance_;
        mirinae::PhysDevice phys_device_;
        mirinae::LogiDevice logi_device_;
        mirinae::Swapchain swapchain_;
        mirinae::Pipeline pipeline_layout_;
        mirinae::RenderPass renderpass_;
        VkSurfaceKHR surface_ = nullptr;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
