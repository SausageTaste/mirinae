#include <memory>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_vulkan.h>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <mirinae/engine.hpp>
#include <mirinae/lightweight/include_spdlog.hpp>
#include <mirinae/render/platform_func.hpp>
#include <sung/basic/os_detect.hpp>


namespace {

#ifdef SUNG_OS_LINUX

    std::filesystem::path get_home_path() {
        const char* homedir;
        if ((homedir = getenv("HOME")) == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }
        return std::filesystem::path(homedir);
    }

#endif

    std::filesystem::path get_documents_path(const char* app_name) {
#ifdef SUNG_OS_WINDOWS
        if (auto pPath = getenv("USERPROFILE")) {
            auto path = std::filesystem::path(pPath) / "Documents" / app_name;

            if (!std::filesystem::is_directory(path)) {
                if (!std::filesystem::create_directories(path)) {
                    MIRINAE_ABORT("Failed to create directory");
                }
            }

            return path;
        }
        MIRINAE_ABORT("Failed to get user profile path");
#elif defined(SUNG_OS_LINUX)
        return ::get_home_path() / "Documents" / app_name;
#endif
    }

}  // namespace


namespace {

    std::filesystem::path find_asset_folder() {
        std::filesystem::path cur_path = ".";

        for (int i = 0; i < 10; ++i) {
            const auto folder_path = cur_path / "asset";
            if (std::filesystem::is_directory(folder_path)) {
                return std::filesystem::absolute(folder_path);
            } else {
                cur_path /= "..";
            }
        }

        MIRINAE_ABORT("Failed to find asset path");
        return "";
    }


    class ImGuiContextRaii {

    public:
        ImGuiContextRaii() {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            (void)io;
        }

        ~ImGuiContextRaii() { ImGui::DestroyContext(); }
    } g_imgui_ctxt_raii;


    class WindowSDL
        : public mirinae::IOsIoFunctions
        , public mirinae::VulkanPlatformFunctions {

    public:
        WindowSDL() {
            if (!SDL_Init(SDL_INIT_VIDEO))
                MIRINAE_ABORT("SDL_Init failed: {}", SDL_GetError());

            auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
            window_ = SDL_CreateWindow("Mirinapp", 1280, 720, flags);
            SDL_ShowWindow(window_);
            ImGui_ImplSDL3_InitForVulkan(window_);
        }

        ~WindowSDL() {
            ImGui_ImplSDL3_Shutdown();

            if (window_)
                SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        bool get_win_size(int& width, int& height) const {
            SDL_GetWindowSize(window_, &width, &height);
            return true;
        }

        void fill_vulkan_extensions(std::vector<std::string>& out) const {
            Uint32 count = 0;
            const auto list = SDL_Vulkan_GetInstanceExtensions(&count);
            for (Uint32 i = 0; i < count; ++i) {
                out.push_back(list[i]);
            }
        }

        bool rel_mouse_mode() const {
            return SDL_GetWindowRelativeMouseMode(window_);
        }

        // IOsIoFunctions

        bool toggle_fullscreen() override {
            const auto fullscreen = SDL_GetWindowFlags(window_) &
                                    SDL_WINDOW_FULLSCREEN;
            return SDL_SetWindowFullscreen(window_, !fullscreen);
        }

        bool set_hidden_mouse_mode(bool hidden) override {
            if (SDL_SetWindowRelativeMouseMode(window_, hidden)) {
                if (hidden) {
                    SPDLOG_DEBUG("Enable relative mouse mode");
                } else {
                    SPDLOG_DEBUG("Disable relative mouse mode");
                }
            } else {
                SPDLOG_ERROR(
                    "Failed to set relative mouse mode: {}", SDL_GetError()
                );
            }
            return true;
        }

        std::optional<std::string> get_clipboard() override {
            return std::nullopt;
        }

        bool set_clipboard(const std::string& text) override { return false; }

        // VulkanPlatformFunctions

        VkSurfaceKHR create_surface(VkInstance instance) override {
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            const auto result = SDL_Vulkan_CreateSurface(
                window_, instance, nullptr, &surface
            );
            MIRINAE_ASSERT(surface != VK_NULL_HANDLE);
            return surface;
        }

        void imgui_new_frame() override { ImGui_ImplSDL3_NewFrame(); }

    private:
        SDL_Window* window_ = nullptr;
    };


    class CombinedEngine {

    public:
        CombinedEngine() {
            system("chcp 65001");
            spdlog::set_level(spdlog::level::level_enum::trace);

            mirinae::EngineCreateInfo cinfo;

            cinfo.filesys_ = std::make_shared<dal::Filesystem>();
            cinfo.filesys_->add_subsys(
                dal::create_filesubsys_std(":asset", ::find_asset_folder())
            );
            cinfo.filesys_->add_subsys(
                dal::create_filesubsys_std("", ::get_documents_path("Mirinapp"))
            );

            window_.fill_vulkan_extensions(cinfo.instance_extensions_);
            window_.get_win_size(cinfo.init_width_, cinfo.init_height_);
            cinfo.osio_ = &window_;
            cinfo.vulkan_os_ = &window_;
            cinfo.ui_scale_ = 1;
            cinfo.enable_validation_layers_ = true;

            engine_ = mirinae::create_engine(std::move(cinfo));
        }

        void do_frame() { engine_->do_frame(); }

        auto& Window() { return window_; }
        auto& engine() { return *engine_; }

    private:
        WindowSDL window_;
        std::unique_ptr<mirinae::IEngine> engine_;
    };

}  // namespace


SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    auto app = std::make_unique<::CombinedEngine>();
    *appstate = app.release();
    return SDL_AppResult::SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppIterate(void* appstate) {
    auto app = static_cast<::CombinedEngine*>(appstate);
    app->do_frame();
    return SDL_AppResult::SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* e) {
    if (e->type == SDL_EVENT_QUIT)
        return SDL_AppResult::SDL_APP_SUCCESS;

    ImGui_ImplSDL3_ProcessEvent(e);
    auto app = static_cast<::CombinedEngine*>(appstate);
    auto& engine = app->engine();

    if (e->type == SDL_EVENT_KEY_DOWN) {
        mirinae::key::Event ke;
        ke.action_type = mirinae::key::ActionType::down;
        ke.scancode_ = e->key.scancode;
        ke.keycode_ = e->key.key;
        SPDLOG_INFO("Key down: {}", ke.scancode_);
        engine.on_key_event(ke);
    } else if (e->type == SDL_EVENT_KEY_UP) {
        mirinae::key::Event ke;
        ke.action_type = mirinae::key::ActionType::up;
        ke.scancode_ = e->key.scancode;
        ke.keycode_ = e->key.key;
        SPDLOG_INFO("Key up: {}", ke.scancode_);
        engine.on_key_event(ke);
    } else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        mirinae::mouse::Event me;
        me.action_ = mirinae::mouse::ActionType::down;
        me.button_ = mirinae::mouse::ButtonCode::right;
        me.xpos_ = e->button.x;
        me.ypos_ = e->button.y;
        SPDLOG_INFO(
            "Mouse button down: ({}, {}), btn={}",
            e->button.x,
            e->button.y,
            e->button.button
        );
        engine.on_mouse_event(me);
    } else if (e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        mirinae::mouse::Event me;
        me.action_ = mirinae::mouse::ActionType::up;
        me.button_ = mirinae::mouse::ButtonCode::right;
        me.xpos_ = e->button.x;
        me.ypos_ = e->button.y;
        SPDLOG_INFO(
            "Mouse button up: ({}, {}), btn={}",
            e->button.x,
            e->button.y,
            e->button.button
        );
        engine.on_mouse_event(me);
    } else if (e->type == SDL_EVENT_MOUSE_MOTION) {
        mirinae::mouse::Event me;
        me.action_ = mirinae::mouse::ActionType::move;
        me.button_ = mirinae::mouse::ButtonCode::eoe;
        me.xpos_ = e->button.x;
        me.ypos_ = e->button.y;
        if (app->Window().rel_mouse_mode())
            SPDLOG_INFO("Rel motion: ({}, {})", e->motion.xrel, e->motion.yrel);
        else
            SPDLOG_INFO("Abs motion: ({}, {})", e->button.x, e->button.y);
        engine.on_mouse_event(me);
    } else {
        SPDLOG_WARN("Unhandled event: {}", e->type);
    }

    return SDL_AppResult::SDL_APP_CONTINUE;
}


void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    std::unique_ptr<::CombinedEngine> app(
        static_cast<::CombinedEngine*>(appstate)
    );
}
