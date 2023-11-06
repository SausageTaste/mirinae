#include <mutex>

#include <jni.h>
#include <android/log.h>
#include <game-activity/GameActivity.cpp>
#include <game-activity/native_app_glue/android_native_app_glue.c>
#include <game-text-input/gametextinput.cpp>

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <mirinae/engine.hpp>

#include "filesys.hpp"


namespace {

    class LogcatSink : public spdlog::sinks::base_sink<std::mutex> {

    public:
        LogcatSink() = default;

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
            const auto text = fmt::to_string(formatted);
            const auto log_level = this->log_level_cast(msg.level);
            __android_log_print(log_level, "Mirinae", "%s\n", text.c_str());
        }

        void flush_() override {

        }

    private:
        static android_LogPriority log_level_cast(spdlog::level::level_enum e) {
            switch (e) {
                case spdlog::level::level_enum::trace:
                    return ANDROID_LOG_VERBOSE;
                case spdlog::level::level_enum::debug:
                    return ANDROID_LOG_DEBUG;
                case spdlog::level::level_enum::info:
                    return ANDROID_LOG_INFO;
                case spdlog::level::level_enum::warn:
                    return ANDROID_LOG_WARN;
                case spdlog::level::level_enum::err:
                    return ANDROID_LOG_ERROR;
                case spdlog::level::level_enum::critical:
                    return ANDROID_LOG_FATAL;
                case spdlog::level::level_enum::off:
                    return ANDROID_LOG_SILENT;
                default:
                    return ANDROID_LOG_UNKNOWN;
            }
        }

    };


    class CombinedEngine {

    public:
        CombinedEngine(android_app* const state) {
            // Logger
            {
                std::vector<spdlog::sink_ptr> sinks;
                sinks.emplace_back(std::make_shared<LogcatSink>());
                auto logger = std::make_shared<spdlog::logger>("Mirinae", sinks.begin(), sinks.end());
                spdlog::set_default_logger(logger);
            }

            create_info_.filesys_ = mirinapp::create_filesys_android_asset(state->activity->assetManager);

            create_info_.instance_extensions_ = std::vector<std::string>{
                    "VK_KHR_surface",
                    "VK_KHR_android_surface",
            };
            create_info_.surface_creator_ = [state](void* instance) -> uint64_t {
                VkAndroidSurfaceCreateInfoKHR create_info{
                        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                        .pNext = nullptr,
                        .flags = 0,
                        .window = state->window,
                };

                VkSurfaceKHR surface = VK_NULL_HANDLE;
                const auto create_result = vkCreateAndroidSurfaceKHR(
                        reinterpret_cast<VkInstance>(instance),
                        &create_info,
                        nullptr,
                        &surface
                );

                return *reinterpret_cast<uint64_t*>(&surface);
            };

            engine_ = mirinae::create_engine(std::move(create_info_));
        }

        void do_frame() {
            engine_->do_frame();
        }

        [[nodiscard]]
        bool is_ongoing() const {
            if (nullptr == engine_)
                return false;
            if (!engine_->is_ongoing())
                return false;

            return true;
        }

    private:
        mirinae::EngineCreateInfo create_info_;
        std::unique_ptr<mirinae::IEngine> engine_;

    };

}


extern "C" {

void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // A new window is created, associate a renderer with it. You may replace this with a
            // "game" class if that suits your needs. Remember to change all instances of userData
            // if you change the class here as a reinterpret_cast is dangerous this in the
            // android_main function and the APP_CMD_TERM_WINDOW handler case.
            pApp->userData = new ::CombinedEngine(pApp);
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being destroyed. Use this to clean up your userData to avoid leaking
            // resources.
            //
            // We have to check if userData is assigned just in case this comes in really quickly
            if (pApp->userData) {
                auto engine = reinterpret_cast<::CombinedEngine*>(pApp->userData);
                delete engine;
            }
            break;
        default:
            break;
    }
}

/*!
 * Enable the motion events you want to handle; not handled events are
 * passed back to OS for further processing. For this example case,
 * only pointer and joystick devices are enabled.
 *
 * @param motionEvent the newly arrived GameActivityMotionEvent.
 * @return true if the event is from a pointer or joystick device,
 *         false for all other input devices.
 */
bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
    auto sourceClass = motionEvent->source & AINPUT_SOURCE_CLASS_MASK;
    return (sourceClass == AINPUT_SOURCE_CLASS_POINTER ||
            sourceClass == AINPUT_SOURCE_CLASS_JOYSTICK);
}

/*!
 * This the main entry point for a native activity
 */
void android_main(struct android_app *pApp) {
    // Register an event handler for Android events
    pApp->onAppCmd = handle_cmd;

    // Set input event filters (set it to NULL if the app wants to process all inputs).
    // Note that for key inputs, this example uses the default default_key_filter()
    // implemented in android_native_app_glue.c.
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    // This sets up a typical game/event loop. It will run until the app is destroyed.
    int events;
    android_poll_source *pSource;
    do {
        // Process all pending events before running game logic.
        if (ALooper_pollAll(0, nullptr, &events, (void **) &pSource) >= 0) {
            if (pSource) {
                pSource->process(pApp, pSource);
            }
        }

        // Check if any user data is associated. This is assigned in handle_cmd
        if (pApp->userData) {
            auto engine = reinterpret_cast<::CombinedEngine*>(pApp->userData);

            if (!engine->is_ongoing())
                break;

            engine->do_frame();
        }
    } while (!pApp->destroyRequested);
}

}
