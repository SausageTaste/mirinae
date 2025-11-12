#include "mirinae/engine.hpp"

#include <deque>

#define MINIAUDIO_IMPLEMENTATION
#include <SDL3/SDL_scancode.h>
#include <miniaudio.h>
#include <spdlog/sinks/base_sink.h>
#include <daltools/common/glm_tool.hpp>
#include <daltools/common/task_sys.hpp>

#include "mirinae/imgui_widget.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/network.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/lua/script.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/renderer.hpp"
#include "mirinae/spawn_entt.hpp"


namespace {

    class NoclipController : public mirinae::IInputProcessor {

    public:
        using TransformQuat = mirinae::TransformQuat<double>;
        using Angle = TransformQuat::Angle;

    public:
        bool on_mouse_event(const mirinae::mouse::Event& e) {
            using mirinae::mouse::ActionType;

            if (e.action_ == ActionType::move) {
                if (move_pointer_ == &mouse_state_) {
                    mouse_state_.last_pos_ = { e.xpos_, e.ypos_ };
                    return true;
                } else if (look_pointer_ == &mouse_state_) {
                    mouse_state_.last_pos_ = { e.xpos_, e.ypos_ };
                    return true;
                }
                return false;
            }

            if (e.button_ == mirinae::mouse::ButtonCode::right) {
                if (e.action_ == ActionType::down) {
                    if (e.xpos_ < 400) {
                        if (move_pointer_)
                            return true;
                        move_pointer_ = &mouse_state_;
                    } else {
                        if (look_pointer_)
                            return true;
                        look_pointer_ = &mouse_state_;
                        if (osio_)
                            osio_->set_hidden_mouse_mode(true);
                    }

                    mouse_state_.start_pos_ = { e.xpos_, e.ypos_ };
                    mouse_state_.last_pos_ = mouse_state_.start_pos_;
                    mouse_state_.consumed_pos_ = mouse_state_.start_pos_;
                    return true;
                } else if (e.action_ == ActionType::up) {
                    if (move_pointer_ == &mouse_state_) {
                        move_pointer_ = nullptr;
                    } else if (look_pointer_ == &mouse_state_) {
                        look_pointer_ = nullptr;
                    }

                    mouse_state_.start_pos_ = { 0, 0 };
                    mouse_state_.last_pos_ = mouse_state_.start_pos_;
                    mouse_state_.consumed_pos_ = mouse_state_.start_pos_;
                    if (osio_)
                        osio_->set_hidden_mouse_mode(false);
                    return true;
                }
            }

            if (move_pointer_ == &mouse_state_)
                return true;
            else if (look_pointer_ == &mouse_state_)
                return true;

            return false;
        }

        bool on_touch_event(const mirinae::touch::Event& e) {
            using mirinae::touch::ActionType;

            if (touch_pointers_.size() < e.index_ + 1)
                touch_pointers_.resize(e.index_ + 1);
            auto& touch_state = touch_pointers_[e.index_];

            if (e.action_ == ActionType::move) {
                if (move_pointer_ == &touch_state) {
                    touch_state.last_pos_ = { e.xpos_, e.ypos_ };
                    return true;
                } else if (look_pointer_ == &touch_state) {
                    touch_state.last_pos_ = { e.xpos_, e.ypos_ };
                    return true;
                }
                return false;
            }

            if (e.action_ == ActionType::down) {
                if (e.xpos_ < 400) {
                    if (move_pointer_)
                        return true;
                    move_pointer_ = &touch_state;
                } else {
                    if (look_pointer_)
                        return true;
                    look_pointer_ = &touch_state;
                }

                touch_state.start_pos_ = { e.xpos_, e.ypos_ };
                touch_state.last_pos_ = touch_state.start_pos_;
                touch_state.consumed_pos_ = touch_state.start_pos_;
                return true;
            } else if (e.action_ == ActionType::up) {
                if (move_pointer_ == &touch_state) {
                    move_pointer_ = nullptr;
                } else if (look_pointer_ == &touch_state) {
                    look_pointer_ = nullptr;
                }

                touch_state.start_pos_ = { 0, 0 };
                touch_state.last_pos_ = touch_state.start_pos_;
                touch_state.consumed_pos_ = touch_state.start_pos_;
                return true;
            }

            if (move_pointer_ == &touch_state)
                return true;
            else if (look_pointer_ == &touch_state)
                return true;

            return false;
        }

        void apply(
            TransformQuat& transform,
            const mirinae::InputActionMapper& action_map,
            const double delta_time
        ) {
            using InputAction = mirinae::InputActionMapper::ActionType;

            {
                glm::dvec3 move_dir{
                    action_map.get_value_move_right(),
                    0,
                    action_map.get_value_move_backward(),
                };

                if (glm::length(move_dir) > 0) {
                    move_dir = glm::mat3_cast(transform.rot_) * move_dir;
                    transform.pos_ += move_dir * (delta_time * move_speed_);
                }
            }

            {
                double vertical = 0;
                if (action_map.get_value(InputAction::translate_up))
                    vertical += 1;
                if (action_map.get_value(InputAction::translate_down))
                    vertical -= 1;

                transform.pos_.y += vertical * delta_time * move_speed_;
            }

            const auto look_rot = action_map.get_value_key_look() +
                                  action_map.get_value_mouse_look();

            {
                auto r = Angle::from_rad(look_rot.x);
                if (0 != r.rad())
                    transform.rotate(
                        r * (delta_time * 2), glm::vec3{ 0, 1, 0 }
                    );
            }

            {
                auto r = Angle::from_rad(look_rot.y);
                if (0 != r.rad()) {
                    const auto right = glm::mat3_cast(transform.rot_) *
                                       glm::vec3{ 1, 0, 0 };
                    transform.rotate(r * (delta_time * 2), right);
                }
            }

            if (move_pointer_) {
                const auto pos_diff = move_pointer_->last_pos_ -
                                      move_pointer_->start_pos_;
                glm::dvec3 move_dir{
                    sung::clamp(pos_diff.x / 50.0, -1.0, 1.0),
                    0,
                    sung::clamp(pos_diff.y / 50.0, -1.0, 1.0)
                };

                if (glm::length(move_dir) > 0) {
                    move_dir = glm::mat3_cast(transform.rot_) * move_dir;
                    transform.pos_ += move_dir * (delta_time * move_speed_);
                }

                move_pointer_->consumed_pos_ = move_pointer_->last_pos_;
            }

            if (look_pointer_) {
                {
                    const auto rot = look_pointer_->consumed_pos_.x -
                                     look_pointer_->last_pos_.x;
                    if (0 != rot)
                        transform.rotate(
                            Angle::from_rad(rot * 0.002), glm::vec3{ 0, 1, 0 }
                        );
                }

                {
                    const auto rot = look_pointer_->consumed_pos_.y -
                                     look_pointer_->last_pos_.y;
                    if (0 != rot) {
                        const auto right = glm::mat3_cast(transform.rot_) *
                                           glm::vec3{ 1, 0, 0 };
                        transform.rotate(Angle::from_rad(rot * 0.002), right);
                    }
                }

                look_pointer_->consumed_pos_ = look_pointer_->last_pos_;
            }
        }

        std::shared_ptr<mirinae::IOsIoFunctions> osio_;

    private:
        class PointerState {

        public:
            glm::dvec2 start_pos_{ 0, 0 };
            glm::dvec2 last_pos_{ 0, 0 };
            glm::dvec2 consumed_pos_{ 0, 0 };
        };

        std::deque<PointerState> touch_pointers_;
        PointerState mouse_state_;
        mirinae::key::EventAnalyzer keys_;
        PointerState* move_pointer_ = nullptr;
        PointerState* look_pointer_ = nullptr;
        double move_speed_ = 10;
    };


    template <size_t TSize>
    class AppendingBuffer {

    public:
        AppendingBuffer() { data_.fill(0); }

        void push(float value) {
            this->shift_left();
            data_[TSize - 1] = value;
        }

        const float* data() const { return data_.data(); }
        size_t size() const { return TSize; }

    private:
        void shift_left() {
            for (size_t i = 1; i < TSize; ++i) {
                data_[i - 1] = data_[i];
            }
        }

        std::array<float, TSize> data_;
    };


    class AudioSystem {

    public:
        AudioSystem() {
            ma_engine_config config;
            config = ma_engine_config_init();

            if (MA_SUCCESS != ma_engine_init(&config, &engine_)) {
                throw std::runtime_error("Failed to initialize audio engine");
            }

            const auto path = "C:/Users/woos8/Downloads/VIBE_20251112/song.mp3";
            const auto result = ma_sound_init_from_file(
                &engine_, path, MA_SOUND_FLAG_DECODE, nullptr, nullptr, &sound_
            );
            if (result != MA_SUCCESS) {
                throw std::runtime_error("Failed to load audio file");
            }

            ma_sound_set_positioning(&sound_, ma_positioning_relative);
            ma_sound_start(&sound_);
        }

        ~AudioSystem() { ma_engine_uninit(&engine_); }

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem(AudioSystem&&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;
        AudioSystem& operator=(AudioSystem&&) = delete;

        void tick() {
            const auto t = sung::get_time_unix();
            ma_sound_set_position(&sound_, 3 * cos(t), 0, 3 * sin(t));
        }

    private:
        ma_engine engine_;
        ma_sound sound_;
    };

}  // namespace


namespace {

    std::vector<std::string> g_texts;

    class MySink : public spdlog::sinks::base_sink<std::mutex> {

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            std::string_view sv{ msg.payload.data(), msg.payload.size() };

            std::string ss;
            for (auto c : sv) {
                if (c == '\n') {
                    g_texts.push_back(ss);
                    ss.clear();
                } else {
                    ss += c;
                }
            }

            if (!ss.empty())
                g_texts.push_back(ss);
        }

        void flush_() override {
            // Do nothing because statement executed in sink_it_().
        }
    };

}  // namespace


namespace {

    class TaskGlobalStateProceed : public dal::ITask {

    public:
        TaskGlobalStateProceed(std::shared_ptr<mirinae::CosmosSimulator> cosmos)
            : cosmos_(cosmos) {}

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            cosmos_->tick_clock();
            cosmos_->scene().do_frame();
        }

        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
    };

}  // namespace


namespace {

    class Engine : public mirinae::IEngine {

    public:
        Engine(mirinae::EngineCreateInfo&& cinfo) {
            ecinfo_ = std::move(cinfo);
            win_width_ = ecinfo_.init_width_;
            win_height_ = ecinfo_.init_height_;

            action_mapper_.give_osio(*ecinfo_.osio_);

            sung::HTaskSche task_sche = sung::create_task_scheduler();
            // client_ = mirinae::create_client();
            script_ = std::make_shared<mirinae::ScriptEngine>();
            cosmos_ = std::make_shared<mirinae::CosmosSimulator>(*script_);

            spdlog::default_logger()->sinks().push_back(
                std::make_shared<MySink>()
            );

            mirinae::register_modules(*script_);
            mirinae::spawn_entities(*cosmos_);

            // Script
            /*
            {
                const auto contents = ecinfo_.filesys_->read_file(
                    ":asset/script/startup.lua"
                );
                if (contents) {
                    const std::string str{ contents->begin(), contents->end() };
                    script_->exec(str.c_str());
                }
            }
            */

            // ImGui Widgets
            {
                imgui_main_ = mirinae::imgui::create_main_win(
                    cosmos_, ecinfo_.filesys_, script_
                );
                cosmos_->imgui_.push_back(imgui_main_);
            }

            cosmos_->register_tasks(tasks_, action_mapper_);

            renderer_ = mirinae::create_vk_renderer(
                ecinfo_, task_sche, cosmos_
            );
            renderer_->register_tasks(tasks_);

            cosmos_->phys_world().give_debug_ren(renderer_->debug_ren());
        }

        ~Engine() override {}

        void do_frame() override {
            audio_.tick();
            tasks_.start();

            /*
            auto cam_view = cosmos_->reg().try_get<mirinae::cpnt::Transform>(
                cosmos_->scene().main_camera_
            );

            auto flashlight_tform =
                cosmos_->reg().try_get<mirinae::cpnt::Transform>(flashlight_);
            if (flashlight_tform) {
                flashlight_tform->pos_ = cam_view->pos_ +
                                         glm::dvec3{ 0, -0.1, 0 };
                flashlight_tform->rot_ = cam_view->rot_;
                flashlight_tform->rotate(
                    sung::TAngle<double>::from_deg(std::atan(0.1 / 5.0)),
                    cam_view->make_right_dir()
                );
            }
            */

            /*
            for (auto e : cosmos_->reg().view<mirinae::cpnt::Envmap>()) {
                auto& tform = cosmos_->reg().get<mirinae::cpnt::Transform>(e);
                tform.pos_ = cam_view->pos_;
            }
            */

            // client_->do_frame();
            renderer_->do_frame();
        }

        bool is_ongoing() override { return true; }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            win_width_ = width;
            win_height_ = height;
            renderer_->notify_window_resize(width, height);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (e.scancode_ == SDL_SCANCODE_GRAVE) {
                if (e.action_type == mirinae::key::ActionType::up)
                    imgui_main_->toggle_show();
                return true;
            }

            if (renderer_->on_key_event(e))
                return true;
            if (action_mapper_.on_key_event(e))
                return true;

            return false;
        }

        bool on_text_event(char32_t c) override {
            return renderer_->on_text_event(c);
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (renderer_->on_mouse_event(e))
                return true;

            /*
            if (e.button_ == mirinae::mouse::ButtonCode::left &&
                e.action_ == mirinae::mouse::ActionType::up) {
                auto& reg = cosmos_->reg();
                const auto e_cam = cosmos_->scene().main_camera_;
                auto cam = reg.try_get<cpnt::StandardCamera>(e_cam);
                if (!cam) {
                    SPDLOG_WARN("No camera entity found.");
                    return true;
                }
                auto tform = reg.try_get<cpnt::Transform>(e_cam);
                if (!tform) {
                    SPDLOG_WARN("A camera entity without Transform component.");
                    return true;
                }

                // Upper left is (-1, -1)
                const glm::vec4 in_ndc{
                    (2.0 * e.xpos_) / win_width_ - 1.0,
                    (2.0 * e.ypos_) / win_height_ - 1.0,
                    -1,
                    1,
                };
                const auto proj_mat = cam->proj_.make_proj_mat(
                    win_width_, win_height_
                );
                const auto proj_inv = glm::inverse(proj_mat);
                auto in_view = proj_inv * in_ndc;
                in_view /= in_view.w;

                const auto view_mat = tform->make_view_mat();
                const auto view_inv = glm::inverse(view_mat);
                const auto in_world = view_inv * in_view;
                const auto dir = glm::dvec3{ in_world } - tform->pos_;

                const sung::LineSegment3 ray{
                    dal::vec_cast(tform->pos_),
                    dal::vec_cast(glm::normalize(dir) * 1000.0),
                };
                cosmos_->scene().pick_entt(ray);
                return true;
            }
            */

            if (action_mapper_.on_mouse_event(e))
                return true;

            return true;
        }

        bool on_mouse_rel_event(const mirinae::mouse::EventRel& e) override {
            if (renderer_->on_mouse_rel_event(e))
                return true;
            if (action_mapper_.on_mouse_rel_event(e))
                return true;

            return true;
        }

        bool on_touch_event(const mirinae::touch::Event& e) override {
            if (renderer_->on_touch_event(e))
                return true;
            if (action_mapper_.on_touch_event(e))
                return true;

            return true;
        }

    private:
        mirinae::EngineCreateInfo ecinfo_;

        // std::unique_ptr<mirinae::INetworkClient> client_;
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
        std::shared_ptr<mirinae::imgui::IMainWin> imgui_main_;
        std::unique_ptr<mirinae::IRenderer> renderer_;

        ::AudioSystem audio_;
        mirinae::TaskGraph tasks_;
        sung::MonotonicRealtimeTimer sec5_;
        mirinae::InputActionMapper action_mapper_;
        entt::entity flashlight_;
        uint32_t win_width_ = 0, win_height_ = 0;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(EngineCreateInfo&& cinfo) {
        return std::make_unique<::Engine>(std::move(cinfo));
    }

}  // namespace mirinae
