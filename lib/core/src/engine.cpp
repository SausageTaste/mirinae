#include "mirinae/engine.hpp"

#include <deque>

#include <imgui.h>
#include <daltools/common/glm_tool.hpp>
#include <sung/basic/cvar.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/ocean.hpp"
#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/network.hpp"
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
            mirinae::TransformQuat<double>& transform, const double delta_time
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
                            mirinae::cpnt::Transform::Angle::from_rad(
                                rot * 0.002
                            ),
                            glm::vec3{ 0, 1, 0 }
                        );
                }

                {
                    const auto rot = look_pointer_->consumed_pos_.y -
                                     look_pointer_->last_pos_.y;
                    if (0 != rot) {
                        const auto right = glm::mat3_cast(transform.rot_) *
                                           glm::vec3{ 1, 0, 0 };
                        transform.rotate(
                            mirinae::cpnt::Transform::Angle::from_rad(
                                rot * 0.002
                            ),
                            right
                        );
                    }
                }

                look_pointer_->consumed_pos_ = look_pointer_->last_pos_;
            }
        }

        auto& keys() const { return keys_; }

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


    class ImGuiEntt : public mirinae::ImGuiRenderUnit {

    public:
        ImGuiEntt(std::shared_ptr<mirinae::CosmosSimulator>& cosmos)
            : cosmos_(cosmos) {}

        void render() {
            using namespace mirinae::cpnt;

            auto& reg = this->reg();
            if (ImGui::Begin("Entities")) {
                if (ImGui::CollapsingHeader("CVars")) {
                    ImGui::Indent(20);
                    this->render_cvar();
                    ImGui::Unindent(20);
                }

                if (ImGui::CollapsingHeader("StandardCamera")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<StandardCamera>())
                        this->render_standard_camera(e);
                    ImGui::Unindent(20);
                }

                for (auto e : this->reg().view<entt::entity>()) {
                    this->render_entt(e);
                }
            }

            ImGui::End();
        }

    private:
        class CvarVisitor : public sung::ICVarVisitor {

        public:
            virtual void visit(sung::ICVarInt& cvar) {
                int value = cvar.get();
                ImGui::PushID(&cvar);
                ImGui::DragInt(cvar.id().c_str(), &value);
                ImGui::PopID();
                cvar.set(value);
            }

            virtual void visit(sung::ICVarFloat& cvar) {
                double value = cvar.get();
                ImGui::PushID(&cvar);
                ImGui::DragScalar(
                    cvar.id().c_str(), ImGuiDataType_Double, &value, 0.1
                );
                ImGui::PopID();
                cvar.set(value);
            }

            virtual void visit(sung::ICVarStr& cvar) {}
        };

        const sung::SimClock& clock() const { return cosmos_->scene().clock(); }

        entt::registry& reg() { return cosmos_->reg(); }

        mirinae::cpnt::Ocean* try_find_ocaen() {
            for (auto& eid : reg().view<mirinae::cpnt::Ocean>()) {
                return &reg().get<mirinae::cpnt::Ocean>(eid);
            }
            return nullptr;
        }

        void render_transform(mirinae::cpnt::Transform& transform) {
            auto transformf = transform.copy<float>();
            glm::vec3 rot{ 0 };

            ImGui::PushID(&transform);
            transform.render_imgui(cosmos_->scene().clock());
            ImGui::PopID();
        }

        void render_cvar() { sung::gcvars().visit(CvarVisitor{}); }

        void render_standard_camera(entt::entity e) {
            using namespace mirinae::cpnt;

            auto p_cam = this->reg().try_get<StandardCamera>(e);
            if (!p_cam)
                return;
            auto& cam = *p_cam;

            const auto name = fmt::format("StandardCamera-{}", (ENTT_ID_TYPE)e);

            if (ImGui::CollapsingHeader(name.c_str())) {
                float angle = (float)cam.proj_.fov_.deg();
                ImGui::SliderFloat(
                    "FOV",
                    &angle,
                    0.01f,
                    179.99f,
                    nullptr,
                    ImGuiSliderFlags_Logarithmic
                );
                cam.proj_.fov_.set_deg(angle);

                float near = (float)cam.proj_.near_;
                ImGui::SliderFloat(
                    "Near",
                    &near,
                    0.001f,
                    30000,
                    nullptr,
                    ImGuiSliderFlags_Logarithmic
                );
                cam.proj_.near_ = near;

                float far = (float)cam.proj_.far_;
                ImGui::SliderFloat(
                    "Far",
                    &far,
                    0.001f,
                    30000,
                    nullptr,
                    ImGuiSliderFlags_Logarithmic
                );
                cam.proj_.far_ = far;

                ImGui::SliderFloat(
                    "Exposure",
                    &cam.exposure_,
                    0,
                    10,
                    nullptr,
                    ImGuiSliderFlags_Logarithmic
                );

                ImGui::SliderFloat("Gamma", &cam.gamma_, 0, 3);
                cam.gamma_ = std::round(cam.gamma_ * 10.f) / 10.f;
            }
        }

        template <typename T>
        void render_cpnt(T& component, const char* name) {
            constexpr auto flags = ImGuiTreeNodeFlags_DefaultOpen;
            const auto h_name = fmt::format(
                "{}###{}", name, fmt::ptr(&component)
            );

            ImGui::Indent(10);
            if (ImGui::CollapsingHeader(h_name.c_str(), flags)) {
                ImGui::Indent(10);
                ImGui::PushID(&component);
                component.render_imgui(this->clock());
                ImGui::PopID();
                ImGui::Unindent(10);
            }
            ImGui::Unindent(10);
        }

        void render_entt(entt::entity e) {
            namespace cpnt = mirinae::cpnt;

            const auto entt_name = fmt::format("Entity {}", (ENTT_ID_TYPE)e);

            if (ImGui::CollapsingHeader(entt_name.c_str())) {
                if (auto c = this->reg().try_get<cpnt::MdlActorStatic>(e))
                    this->render_cpnt(*c, "Static Actor");
                if (auto c = this->reg().try_get<cpnt::MdlActorSkinned>(e))
                    this->render_cpnt(*c, "Skinned Actor");
                if (auto c = this->reg().try_get<cpnt::DLight>(e))
                    this->render_cpnt(*c, "Directional Light");
                if (auto c = this->reg().try_get<cpnt::SLight>(e))
                    this->render_cpnt(*c, "Spotlight");
                if (auto c = this->reg().try_get<cpnt::VPLight>(e))
                    this->render_cpnt(*c, "Volumetric Point Light");
                if (auto c = this->reg().try_get<cpnt::AtmosphereSimple>(e))
                    this->render_cpnt(*c, "Atmosphere Simple");
                if (auto c = this->reg().try_get<cpnt::Transform>(e))
                    this->render_cpnt(*c, "Transform");
                if (auto c = this->reg().try_get<cpnt::Ocean>(e))
                    this->render_cpnt(*c, "Ocean");
            }
        }

        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
        int cascade_ = 0;
        bool play_ocean_ = true;
    };

}  // namespace


namespace {

    class Engine : public mirinae::IEngine {

    public:
        Engine(mirinae::EngineCreateInfo&& cinfo) {
            win_width_ = cinfo.init_width_;
            win_height_ = cinfo.init_height_;

            filesys_ = cinfo.filesys_;
            camera_controller_.osio_ = cinfo.osio_;

            sung::HTaskSche task_sche = sung::create_task_scheduler();
            client_ = mirinae::create_client();
            script_ = std::make_shared<mirinae::ScriptEngine>();
            cosmos_ = std::make_shared<mirinae::CosmosSimulator>(*script_);
            renderer_ = mirinae::create_vk_renderer(
                cinfo, task_sche, script_, cosmos_
            );

            auto& reg = cosmos_->reg();

            // DLight
            {
                const auto entt = reg.create();
                auto& d = reg.emplace<mirinae::cpnt::DLight>(entt);
                d.color_.set_scaled_color(2, 2, 2);

                auto& t = reg.emplace<mirinae::cpnt::Transform>(entt);
                d.set_light_dir(-0.5613, -0.7396, -0.3713, t);
            }

            // SLight
            {
                flashlight_ = reg.create();
                auto& s = reg.emplace<mirinae::cpnt::SLight>(flashlight_);
                s.color_.set_scaled_color(0, 0, 0);
                s.inner_angle_.set_deg(10);
                s.outer_angle_.set_deg(25);

                auto& t = reg.emplace<mirinae::cpnt::Transform>(flashlight_);
                t.pos_ = { 0, 2, 0 };
            }

            // VPLight
            {
                static const std::array<glm::dvec3, 8> positions{
                    glm::dvec3(23.06373011, 7.406594568543, -40.16145784411),
                    glm::dvec3(-12.5902652420, 3.742476132263, -57.91384509770),
                    glm::dvec3(-27.18744272, 2.81600329, -59.14043760),
                    glm::dvec3(-11.314874664, 4.1150336202, -72.997129503),
                    glm::dvec3(406.275784993, 26.4602983105, -211.201358558),
                    glm::dvec3(17.7235240304, 4.51899223978, -70.447537981),
                    glm::dvec3(-3.62620977943, 0.758235418969, -0.122477245173)
                };

                static const std::array<glm::vec3, 8> colors{
                    glm::vec3(7, 24, 7) * 0.5f,  glm::vec3(24, 18, 7) * 0.5f,
                    glm::vec3(24, 18, 7) * 0.5f, glm::vec3(24, 18, 7) * 0.5f,
                    glm::vec3(240, 180, 70),     glm::vec3(7, 18, 24) * 2.f,
                    glm::vec3(1, 1, 1) * 5.f
                };

                for (size_t i = 0; i < positions.size(); ++i) {
                    const auto e = reg.create();

                    auto& l = reg.emplace<mirinae::cpnt::VPLight>(e);
                    l.color_.set_scaled_color(colors[i]);

                    auto& t = reg.emplace<mirinae::cpnt::Transform>(e);
                    t.pos_ = positions[i];
                }
            }

            // Main Camera
            {
                const auto entt = reg.create();
                cosmos_->scene().main_camera_ = entt;

                auto& d = reg.emplace<mirinae::cpnt::StandardCamera>(entt);

                d.view_.pos_ = {
                    1.9518,
                    0.6559,
                    0.0913,
                };
                d.view_.rot_ = glm::normalize(
                    glm::dquat{
                        -0.921,
                        -0.051,
                        0.3848,
                        -0.021,
                    } *
                    5.0
                );
                d.proj_.near_ = 0.1;
                d.proj_.far_ = 1000;
            }

            // Ocean
            {
                const auto entt = reg.create();
                auto& ocean = reg.emplace<mirinae::cpnt::Ocean>(entt);
                ocean.height_ = 2;
            }

            // Atmosphere
            {
                const auto entt = reg.create();
                auto& atm = reg.emplace<mirinae::cpnt::AtmosphereSimple>(entt);
                atm.fog_color_ = { 0.556, 0.707, 0.846 };
            }

            // Script
            {
                const auto contents = filesys_->read_file(
                    ":asset/script/startup.lua"
                );
                if (contents) {
                    const std::string str{ contents->begin(), contents->end() };
                    script_->exec(str.c_str());
                }
            }

            // ImGui Widgets
            {
                cosmos_->imgui_.push_back(std::make_shared<ImGuiEntt>(cosmos_));
            }
        }

        ~Engine() override {}

        void do_frame() override {
            cosmos_->tick_clock();
            auto& clock = cosmos_->clock();

            if (sec5_.check_if_elapsed(5)) {
                client_->send();
            }

            auto& cam = cosmos_->reg().get<mirinae::cpnt::StandardCamera>(
                cosmos_->scene().main_camera_
            );
            camera_controller_.apply(cam.view_, clock.dt());

            client_->do_frame();
            cosmos_->do_frame();
            renderer_->do_frame();
        }

        bool is_ongoing() override { return true; }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            win_width_ = width;
            win_height_ = height;
            renderer_->notify_window_resize(width, height);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (e.key == mirinae::key::KeyCode::f) {
                auto& reg = cosmos_->reg();
                auto& slight = reg.get<mirinae::cpnt::SLight>(flashlight_);
                if (e.action_type == mirinae::key::ActionType::down) {
                    if (slight.color_.intensity() == 0)
                        slight.color_.set_scaled_color(5, 5, 5);
                    else
                        slight.color_.intensity() = 0;
                }
                return true;
            }

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

            if (e.button_ == mirinae::mouse::ButtonCode::left &&
                e.action_ == mirinae::mouse::ActionType::up) {
                auto cam =
                    cosmos_->reg().try_get<mirinae::cpnt::StandardCamera>(
                        cosmos_->scene().main_camera_
                    );
                if (!cam) {
                    SPDLOG_WARN("No camera entity found.");
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

                const auto view_mat = cam->view_.make_view_mat();
                const auto view_inv = glm::inverse(view_mat);
                const auto in_world = view_inv * in_view;
                const auto dir = glm::dvec3{ in_world } - cam->view_.pos_;

                const sung::LineSegment3 ray{
                    dal::vec_cast(cam->view_.pos_),
                    dal::vec_cast(glm::normalize(dir) * 1000.0),
                };
                cosmos_->scene().pick_entt(ray);
                return true;
            }

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

        bool on_touch_event(const mirinae::touch::Event& e) override {
            if (renderer_->on_touch_event(e))
                return true;
            if (camera_controller_.on_touch_event(e))
                return true;

            return true;
        }

    private:
        std::shared_ptr<dal::Filesystem> filesys_;
        std::unique_ptr<mirinae::INetworkClient> client_;
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
        std::unique_ptr<mirinae::IRenderer> renderer_;

        sung::MonotonicRealtimeTimer delta_timer_, sec5_;
        ::NoclipController camera_controller_;
        entt::entity flashlight_;
        uint32_t win_width_ = 0, win_height_ = 0;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(EngineCreateInfo&& cinfo) {
        return std::make_unique<::Engine>(std::move(cinfo));
    }

}  // namespace mirinae
