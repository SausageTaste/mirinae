#include "mirinae/engine.hpp"

#include "mirinae/renderer.hpp"


namespace {

    class NoclipController : public mirinae::IInputProcessor {

    public:
        bool on_key_event(const mirinae::key::Event& e) {
            keys_.notify(e);

            if (e.action_type == mirinae::key::ActionType::down) {
                if (e.key == mirinae::key::KeyCode::lbracket)
                    move_speed_ *= 0.5;
                else if (e.key == mirinae::key::KeyCode::rbracket)
                    move_speed_ *= 2;
            }

            return true;
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) {
            using mirinae::mouse::ActionType;

            if (e.action_ == ActionType::move && owning_mouse_) {
                last_mouse_pos_ = { e.xpos_, e.ypos_ };
                return true;
            }

            if (e.button_ == mirinae::mouse::ButtonCode::right) {
                if (e.action_ == ActionType::down) {
                    owning_mouse_ = true;
                    last_mouse_pos_ = { e.xpos_, e.ypos_ };
                    last_applied_mouse_pos_ = last_mouse_pos_;
                    if (osio_)
                        osio_->set_hidden_mouse_mode(true);
                    return true;
                } else if (e.action_ == ActionType::up) {
                    owning_mouse_ = false;
                    last_mouse_pos_ = { 0, 0 };
                    last_applied_mouse_pos_ = last_mouse_pos_;
                    if (osio_)
                        osio_->set_hidden_mouse_mode(false);
                    return true;
                }
            }

            return owning_mouse_;
        }

        void apply(
            mirinae::cpnt::Transform& transform, const double delta_time
        ) {
            {
                glm::dvec3 move_dir{ 0, 0, 0 };
                if (keys_.is_pressed(mirinae::key::KeyCode::w))
                    move_dir.z -= 1;
                if (keys_.is_pressed(mirinae::key::KeyCode::s))
                    move_dir.z += 1;
                if (keys_.is_pressed(mirinae::key::KeyCode::a))
                    move_dir.x -= 1;
                if (keys_.is_pressed(mirinae::key::KeyCode::d))
                    move_dir.x += 1;

                if (glm::length(move_dir) > 0) {
                    move_dir = glm::mat3_cast(transform.rot_) * move_dir;
                    transform.pos_ += move_dir * (delta_time * move_speed_);
                }
            }

            {
                double vertical = 0;
                if (keys_.is_pressed(mirinae::key::KeyCode::lctrl))
                    vertical -= 1;
                if (keys_.is_pressed(mirinae::key::KeyCode::space))
                    vertical += 1;

                if (vertical != 0)
                    transform.pos_.y += vertical * delta_time * move_speed_;
            }

            {
                auto rot = mirinae::cpnt::Transform::Angle::from_zero();
                if (keys_.is_pressed(mirinae::key::KeyCode::left))
                    rot = rot.add_rad(1);
                if (keys_.is_pressed(mirinae::key::KeyCode::right))
                    rot = rot.add_rad(-1);

                if (0 != rot.rad())
                    transform.rotate(
                        rot * (delta_time * 2), glm::vec3{ 0, 1, 0 }
                    );
            }

            {
                auto rot = mirinae::cpnt::Transform::Angle::from_zero();
                if (keys_.is_pressed(mirinae::key::KeyCode::up))
                    rot = rot.add_rad(1);
                if (keys_.is_pressed(mirinae::key::KeyCode::down))
                    rot = rot.add_rad(-1);

                if (0 != rot.rad()) {
                    const auto right = glm::mat3_cast(transform.rot_) *
                                       glm::vec3{ 1, 0, 0 };
                    transform.rotate(rot * (delta_time * 2), right);
                }
            }

            {
                const auto rot = last_applied_mouse_pos_.x - last_mouse_pos_.x;
                if (0 != rot)
                    transform.rotate(
                        mirinae::cpnt::Transform::Angle::from_rad(rot * 0.002),
                        glm::vec3{ 0, 1, 0 }
                    );
            }

            {
                const auto rot = last_applied_mouse_pos_.y - last_mouse_pos_.y;
                if (0 != rot) {
                    const auto right = glm::mat3_cast(transform.rot_) *
                                       glm::vec3{ 1, 0, 0 };
                    transform.rotate(
                        mirinae::cpnt::Transform::Angle::from_rad(rot * 0.002),
                        right
                    );
                }
            }

            last_applied_mouse_pos_ = last_mouse_pos_;
        }

        std::shared_ptr<mirinae::IOsIoFunctions> osio_;

    private:
        mirinae::key::EventAnalyzer keys_;
        glm::dvec2 last_mouse_pos_{ 0, 0 };
        glm::dvec2 last_applied_mouse_pos_{ 0, 0 };
        double move_speed_ = 2;
        bool owning_mouse_ = false;
    };

}  // namespace


namespace {

    class Engine : public mirinae::IEngine {

    public:
        Engine(mirinae::EngineCreateInfo&& cinfo) {
            auto filesys = cinfo.filesys_;
            camera_controller_.osio_ = cinfo.osio_;

            script_ = std::make_shared<mirinae::ScriptEngine>();
            cosmos_ = std::make_shared<mirinae::CosmosSimulator>(*script_);
            renderer_ = mirinae::create_vk_renderer(
                std::move(cinfo), script_, cosmos_
            );

            auto& reg = cosmos_->reg();

            // DLight
            {
                const auto entt = reg.create();
                auto& d = reg.emplace<mirinae::cpnt::DLight>(entt);
                d.color_ = glm::vec3{ 5, 5, 5 };
            }

            // SLight
            {
                const auto entt = reg.create();
                auto& s = reg.emplace<mirinae::cpnt::SLight>(entt);
                s.transform_.pos_ = { 0, 2, 0 };
                s.color_ = glm::vec3{ 5, 5, 5 };
                s.inner_angle_.set_deg(10);
                s.outer_angle_.set_deg(25);
            }

            // Main Camera
            {
                const auto entt = reg.create();
                cosmos_->scene().main_camera_ = entt;

                auto& d = reg.emplace<mirinae::cpnt::StandardCamera>(entt);

                d.view_.pos_ = glm::dvec3{ 0.14983922321477,
                                           0.66663010560478,
                                           -1.1615585516897 };
                d.view_.rot_ = { 0.5263130886922,
                                 0.022307853585388,
                                 0.84923568828777,
                                 -0.035994972955897 };
            }

            // Script
            {
                const auto contents = filesys->read_file_to_vector(
                    "asset/script/startup.lua"
                );
                if (contents) {
                    const std::string str{ contents->begin(), contents->end() };
                    script_->exec(str.c_str());
                }
            }
        }

        ~Engine() override {}

        void do_frame() override {
            const auto delta_time = delta_timer_.check_get_elapsed();

            auto& cam = cosmos_->reg().get<mirinae::cpnt::StandardCamera>(
                cosmos_->scene().main_camera_
            );
            camera_controller_.apply(cam.view_, delta_time);

            cosmos_->do_frame();
            renderer_->do_frame();
        }

        bool is_ongoing() override { return true; }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            renderer_->notify_window_resize(width, height);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (renderer_->on_key_event(e))
                return true;
            if (camera_controller_.on_key_event(e))
                return true;

            return false;
        }

        bool on_text_event(char32_t c) override {
            return renderer_->on_text_event(c);
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (renderer_->on_mouse_event(e))
                return true;

            camera_controller_.on_mouse_event(e);

            auto cam = cosmos_->reg().try_get<mirinae::cpnt::StandardCamera>(
                cosmos_->scene().main_camera_
            );
            if (cam) {
                constexpr auto FACTOR = 1.05;
                if (e.action_ == mirinae::mouse::ActionType::mwheel_up)
                    cam->proj_.multiply_fov(1.0 / FACTOR);
                else if (e.action_ == mirinae::mouse::ActionType::mwheel_down)
                    cam->proj_.multiply_fov(FACTOR);
            }

            return true;
        }

    private:
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
        std::unique_ptr<mirinae::IRenderer> renderer_;

        sung::MonotonicClock delta_timer_;
        ::NoclipController camera_controller_;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(EngineCreateInfo&& cinfo) {
        return std::make_unique<::Engine>(std::move(cinfo));
    }

}  // namespace mirinae