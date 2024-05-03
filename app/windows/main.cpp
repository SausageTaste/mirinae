#include "mirinae/engine.hpp"

#include <filesystem>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "dump.hpp"


namespace {

    std::filesystem::path get_windows_documents_path(const char* app_name) {
        if (auto pPath = getenv("USERPROFILE")) {
            auto path = std::filesystem::path(pPath) / "Documents" / app_name;

            if (!std::filesystem::is_directory(path)) {
                if (!std::filesystem::create_directories(path)) {
                    throw std::runtime_error("Failed to create directory");
                }
            }

            return path;
        }

        throw std::runtime_error("Failed to get user profile path");
    }

    auto get_glfw_extensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        return std::vector<std::string>{ glfwExtensions,
                                         glfwExtensions + glfwExtensionCount };
    }


    class GlfwRaii {

    public:
        GlfwRaii() {
            glfwInit();
            spdlog::set_level(spdlog::level::level_enum::trace);
        }

        ~GlfwRaii() { glfwTerminate(); }

    } g_glfw_raii;


    class GlfwWindow {

    public:
        GlfwWindow(int width, int height, const char* title) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
            window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);

            glfwSetFramebufferSizeCallback(window_, callback_fbuf_size);
            glfwSetKeyCallback(window_, callback_key);
            glfwSetMouseButtonCallback(window_, callback_mouse);
            glfwSetCursorPosCallback(window_, callback_cursor_pos);

            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        ~GlfwWindow() {
            if (nullptr != window_) {
                glfwDestroyWindow(window_);
                window_ = nullptr;
            }
        }

        void swap_buffer() { glfwSwapBuffers(window_); }

        bool is_ongoing() const { return !glfwWindowShouldClose(window_); }

        VkSurfaceKHR create_surface(const VkInstance instance) {
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            const auto result = glfwCreateWindowSurface(
                instance, window_, nullptr, &surface
            );

            if (VK_SUCCESS != result) {
                spdlog::error("Failed to create window surface");
                return VK_NULL_HANDLE;
            }
            return surface;
        }

        std::pair<int, int> get_fbuf_size() const {
            std::pair<int, int> output{ 0, 0 };
            if (nullptr == window_)
                return output;

            glfwGetFramebufferSize(window_, &output.first, &output.second);
            return output;
        }

        void notify_should_close() { glfwSetWindowShouldClose(window_, true); }

        void set_userdata(void* ptr) { glfwSetWindowUserPointer(window_, ptr); }

        void set_hidden_mouse_mode(bool hidden) {
            if (hidden) {
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }

    private:
        static void callback_fbuf_size(
            GLFWwindow* window, int width, int height
        ) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);
            engine->notify_window_resize(width, height);
        }

        static void callback_key(
            GLFWwindow* window, int key, int scancode, int action, int mods
        ) {
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

        static void callback_mouse(
            GLFWwindow* window, int button, int action, int mods
        ) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);

            mirinae::mouse::Event e;
            switch (action) {
                case GLFW_RELEASE:
                    e.action_ = mirinae::mouse::ActionType::up;
                    break;
                case GLFW_PRESS:
                default:
                    e.action_ = mirinae::mouse::ActionType::down;
                    break;
            }

            switch (button) {
                case GLFW_MOUSE_BUTTON_LEFT:
                    e.button_ = mirinae::mouse::ButtonCode::left;
                    break;
                case GLFW_MOUSE_BUTTON_RIGHT:
                    e.button_ = mirinae::mouse::ButtonCode::right;
                    break;
                case GLFW_MOUSE_BUTTON_MIDDLE:
                    e.button_ = mirinae::mouse::ButtonCode::middle;
                    break;
                default:
                    spdlog::warn("Unknown mouse button type: {}", button);
                    e.button_ = mirinae::mouse::ButtonCode::eoe;
                    break;
            }

            glfwGetCursorPos(window, &e.xpos_, &e.ypos_);
            engine->notify_mouse_event(e);
        }

        static void callback_cursor_pos(
            GLFWwindow* window, double xpos, double ypos
        ) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);

            mirinae::mouse::Event e;
            e.action_ = mirinae::mouse::ActionType::move;
            e.xpos_ = xpos;
            e.ypos_ = ypos;

            engine->notify_mouse_event(e);
        }

        static mirinae::key::KeyCode map_key_code(const int glfw_key) {
            using mirinae::key::KeyCode;

            if (GLFW_KEY_A <= glfw_key && glfw_key <= GLFW_KEY_Z) {
                auto index = glfw_key - GLFW_KEY_A + int(KeyCode::a);
                return KeyCode(index);
            } else if (GLFW_KEY_0 <= glfw_key && glfw_key <= GLFW_KEY_9) {
                auto index = glfw_key - GLFW_KEY_0 + int(KeyCode::n0);
                return KeyCode(index);
            } else {
                static const std::unordered_map<uint32_t, KeyCode> map{
                    { GLFW_KEY_GRAVE_ACCENT, KeyCode::backquote },
                    { GLFW_KEY_MINUS, KeyCode::minus },
                    { GLFW_KEY_EQUAL, KeyCode::equal },
                    { GLFW_KEY_LEFT_BRACKET, KeyCode::lbracket },
                    { GLFW_KEY_RIGHT_BRACKET, KeyCode::rbracket },
                    { GLFW_KEY_BACKSLASH, KeyCode::backslash },
                    { GLFW_KEY_SEMICOLON, KeyCode::semicolon },
                    { GLFW_KEY_APOSTROPHE, KeyCode::quote },
                    { GLFW_KEY_COMMA, KeyCode::comma },
                    { GLFW_KEY_PERIOD, KeyCode::period },
                    { GLFW_KEY_SLASH, KeyCode::slash },

                    { GLFW_KEY_SPACE, KeyCode::space },
                    { GLFW_KEY_ENTER, KeyCode::enter },
                    { GLFW_KEY_BACKSPACE, KeyCode::backspace },
                    { GLFW_KEY_TAB, KeyCode::tab },

                    { GLFW_KEY_ESCAPE, KeyCode::escape },
                    { GLFW_KEY_LEFT_SHIFT, KeyCode::lshfit },
                    { GLFW_KEY_RIGHT_SHIFT, KeyCode::rshfit },
                    { GLFW_KEY_LEFT_CONTROL, KeyCode::lctrl },
                    { GLFW_KEY_RIGHT_CONTROL, KeyCode::rctrl },
                    { GLFW_KEY_LEFT_ALT, KeyCode::lalt },
                    { GLFW_KEY_RIGHT_ALT, KeyCode::ralt },
                    { GLFW_KEY_UP, KeyCode::up },
                    { GLFW_KEY_DOWN, KeyCode::down },
                    { GLFW_KEY_LEFT, KeyCode::left },
                    { GLFW_KEY_RIGHT, KeyCode::right },
                };

                auto res = map.find(glfw_key);
                if (res == map.end())
                    return KeyCode::eoe;
                else
                    return res->second;
            }
        }

        GLFWwindow* window_ = nullptr;
    };


    class OsInputOutput : public mirinae::IOsIoFunctions {

    public:
        OsInputOutput(GlfwWindow& window) : window_(window) {}

        bool set_hidden_mouse_mode(bool hidden) override {
            window_.set_hidden_mouse_mode(hidden);
            return true;
        }

    private:
        GlfwWindow& window_;
    };


    class CombinedEngine {

    public:
        CombinedEngine() : window_(800, 600, "Mirinapp") {
            mirinae::EngineCreateInfo create_info;
            create_info.filesys_ = mirinae::create_filesys_std(
                ::get_windows_documents_path("Mirinapp")
            );
            create_info.osio_ = std::make_unique<OsInputOutput>(window_);
            create_info.instance_extensions_ = ::get_glfw_extensions();
            create_info.surface_creator_ = [this](void* instance) -> uint64_t {
                auto surface = this->window_.create_surface(
                    reinterpret_cast<VkInstance>(instance)
                );
                return *reinterpret_cast<uint64_t*>(&surface);
            };
            create_info.enable_validation_layers_ = true;
            create_info.init_width_ = 800;
            create_info.init_height_ = 600;

            engine_ = mirinae::create_engine(std::move(create_info));
            window_.set_userdata(engine_.get());
        }

        void do_frame() {
            glfwPollEvents();
            engine_->do_frame();
            window_.swap_buffer();
        }

        bool is_ongoing() const {
            if (!window_.is_ongoing())
                return false;
            if (nullptr == engine_)
                return false;
            if (!engine_->is_ongoing())
                return false;

            return true;
        }

    private:
        GlfwWindow window_;
        std::unique_ptr<mirinae::IEngine> engine_;
    };


    int start() {
        CombinedEngine engine;
        while (engine.is_ongoing()) engine.do_frame();
        return 0;
    }

}  // namespace


int main() {
    __try {
        return ::start();
    } __except (
        mirinae::windows::create_minidump(GetExceptionInformation()),
        EXCEPTION_EXECUTE_HANDLER
    ) {
        std::abort();
    }
}
