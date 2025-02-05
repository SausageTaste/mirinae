#include "mirinae/engine.hpp"

#include <deque>

#include <imgui.h>
#include <daltools/common/glm_tool.hpp>
#include <sung/basic/cvar.hpp>

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

                if (ImGui::CollapsingHeader("StaticModelActor")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<StaticModelActor>())
                        this->render_static_model_actor(e);
                    ImGui::Unindent(20);
                }

                if (ImGui::CollapsingHeader("SkinnedModelActor")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<SkinnedModelActor>())
                        this->render_skinned_model_actor(e);
                    ImGui::Unindent(20);
                }

                if (ImGui::CollapsingHeader("DLight")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<DLight>())
                        this->render_dlight(e);
                    ImGui::Unindent(20);
                }

                if (ImGui::CollapsingHeader("SLight")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<SLight>())
                        this->render_dlight(e);
                    ImGui::Unindent(20);
                }

                if (ImGui::CollapsingHeader("VPLight")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<VPLight>())
                        this->render_vplight(e);
                    ImGui::Unindent(20);
                }

                if (ImGui::CollapsingHeader("Ocean")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<Ocean>())
                        this->render_ocean(e);
                    ImGui::Unindent(20);
                }

                if (ImGui::CollapsingHeader("AtmosphereSimple")) {
                    ImGui::Indent(20);
                    for (const auto e : reg.view<AtmosphereSimple>())
                        this->render_atmosphere(e);
                    ImGui::Unindent(20);
                }
            }

            ImGui::End();

            if (play_ocean_) {
                if (auto ocean = try_find_ocaen()) {
                    ocean->time_ += cosmos_->scene().clock().dt();
                }
            }
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

        entt::registry& reg() { return cosmos_->reg(); }

        mirinae::cpnt::Ocean* try_find_ocaen() {
            for (auto& eid : reg().view<mirinae::cpnt::Ocean>()) {
                return &reg().get<mirinae::cpnt::Ocean>(eid);
            }
            return nullptr;
        }

        static void render_transform(mirinae::cpnt::Transform& transform) {
            auto transformf = transform.copy<float>();
            glm::vec3 rot{ 0 };

            ImGui::DragFloat3("Pos", &transformf.pos_[0]);
            ImGui::DragFloat3("Rot", &rot[0]);
            if (ImGui::Button("Reset rotation"))
                transformf.rot_ = glm::quat(1, 0, 0, 0);
            ImGui::DragFloat3("Scale", &transformf.scale_[0]);

            transform = transformf.copy<double>();
            transform.rotate(
                mirinae::cpnt::Transform::Angle::from_deg(rot.x),
                glm::vec3{ 1, 0, 0 }
            );
            transform.rotate(
                mirinae::cpnt::Transform::Angle::from_deg(rot.y),
                glm::vec3{ 0, 1, 0 }
            );
            transform.rotate(
                mirinae::cpnt::Transform::Angle::from_deg(rot.z),
                glm::vec3{ 0, 0, 1 }
            );
        }

        static void render_color_intensity(mirinae::cpnt::ColorIntensity& ci) {
            ImGui::ColorEdit3("Color", &ci.color()[0]);

            ImGui::SliderFloat(
                "Intensity",
                &ci.intensity(),
                0.0,
                1000.0,
                nullptr,
                ImGuiSliderFlags_Logarithmic
            );
        }

        void render_cvar() {
            sung::gcvars().visit(CvarVisitor{});
        }

        void render_standard_camera(entt::entity e) {
            using namespace mirinae::cpnt;

            auto p_cam = this->reg().try_get<StandardCamera>(e);
            if (!p_cam)
                return;
            auto& cam = *p_cam;

            const auto name = fmt::format("StandardCamera-{}", (ENTT_ID_TYPE)e);

            if (ImGui::CollapsingHeader(name.c_str())) {
                this->render_transform(cam.view_);

                float angle = cam.proj_.fov_.rad();
                ImGui::SliderAngle("FOV", &angle, 1, 179);
                cam.proj_.fov_.set_rad(angle);

                float near = cam.proj_.near_;
                ImGui::SliderFloat(
                    "Near",
                    &near,
                    0.001f,
                    30000,
                    nullptr,
                    ImGuiSliderFlags_Logarithmic
                );
                cam.proj_.near_ = near;

                float far = cam.proj_.far_;
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

        void render_static_model_actor(entt::entity e) {
            using namespace mirinae::cpnt;

            auto p_actor = this->reg().try_get<StaticModelActor>(e);
            if (!p_actor)
                return;
            auto& actor = *p_actor;

            const auto name = fmt::format(
                "StaticModelActor-{}", (ENTT_ID_TYPE)e
            );

            if (ImGui::CollapsingHeader(name.c_str())) {
                ImGui::Text(
                    "Model path: %s", actor.model_path_.u8string().c_str()
                );

                if (auto transform = this->reg().try_get<Transform>(e))
                    this->render_transform(*transform);
            }
        }

        void render_skinned_model_actor(entt::entity e) {
            using namespace mirinae::cpnt;

            auto p_actor = this->reg().try_get<SkinnedModelActor>(e);
            if (!p_actor)
                return;
            auto& actor = *p_actor;

            const auto name = fmt::format(
                "SkinnedModelActor-{}", (ENTT_ID_TYPE)e
            );

            if (ImGui::CollapsingHeader(name.c_str())) {
                ImGui::Text(
                    "Model path: %s", actor.model_path_.u8string().c_str()
                );

                if (auto transform = this->reg().try_get<Transform>(e))
                    this->render_transform(*transform);

                auto& anim = actor.anim_state_;

                float anim_speed = anim.play_speed();
                ImGui::SliderFloat("Animation speed", &anim_speed, 0.0, 10.0);
                anim.set_play_speed(anim_speed);

                const auto anim_name = anim.get_cur_anim_name();
                if (anim_name) {
                    ImGui::Text("Current animation: %s", anim_name->c_str());
                }

                if (ImGui::Button("Next animation")) {
                    const auto anim_count = anim.anims().size();
                    if (anim_count > 0) {
                        const auto cur_idx = anim.get_cur_anim_idx();
                        if (cur_idx) {
                            const auto next_idx = (*cur_idx + 1) % anim_count;
                            anim.select_anim_index(
                                next_idx, cosmos_->scene().clock()
                            );
                        } else {
                            anim.select_anim_index(0, cosmos_->scene().clock());
                        }
                    }
                }

                auto selected_index = anim.get_cur_anim_idx().value_or(0);
                const auto prev_anim_name = anim.get_cur_anim_name().value_or(
                    "None"
                );
                ImGui::InputText(
                    "Search Animation",
                    search_buffer_.data(),
                    search_buffer_.size()
                );

                if (ImGui::BeginCombo("Animations", prev_anim_name.c_str())) {
                    for (int i = 0; i < anim.anims().size(); i++) {
                        const auto& i_anim = anim.anims().at(i);
                        const auto similarity = std::strstr(
                            i_anim.name_.c_str(), search_buffer_.data()
                        );
                        if (similarity) {  // Filter based on input
                            bool is_selected = (selected_index == i);
                            if (ImGui::Selectable(
                                    anim.anims()[i].name_.c_str(), is_selected
                                )) {
                                selected_index = i;
                                anim.select_anim_index(
                                    i, cosmos_->scene().clock()
                                );
                            }
                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }

        void render_dlight(entt::entity e) {
            using namespace mirinae::cpnt;

            auto dlight = this->reg().try_get<DLight>(e);
            if (!dlight)
                return;

            const auto name = fmt::format("DLight-{}", (ENTT_ID_TYPE)e);

            if (ImGui::CollapsingHeader(name.c_str())) {
                this->render_transform(dlight->transform_);
                this->render_color_intensity(dlight->color_);
            }
        }

        void render_slight(entt::entity e) {
            using namespace mirinae::cpnt;

            auto slight = this->reg().try_get<SLight>(e);
            if (!slight)
                return;

            const auto name = fmt::format("SLight-{}", (ENTT_ID_TYPE)e);

            if (ImGui::CollapsingHeader(name.c_str())) {
                this->render_transform(slight->transform_);
                this->render_color_intensity(slight->color_);

                float inner_angle = slight->inner_angle_.rad();
                ImGui::SliderAngle("Inner angle", &inner_angle, 0, 180);
                slight->inner_angle_.set_rad(inner_angle);

                float outer_angle = slight->outer_angle_.rad();
                ImGui::SliderAngle("Outer angle", &outer_angle, 0, 180);
                slight->outer_angle_.set_rad(outer_angle);
            }
        }

        void render_vplight(entt::entity e) {
            using namespace mirinae::cpnt;

            auto vplight = this->reg().try_get<VPLight>(e);
            if (!vplight)
                return;

            const auto name = fmt::format("VPLight-{}", (ENTT_ID_TYPE)e);

            if (ImGui::CollapsingHeader(name.c_str())) {
                glm::vec3 pos = vplight->pos_;
                ImGui::DragFloat3("Position", &pos[0]);
                vplight->pos_ = pos;

                this->render_color_intensity(vplight->color_);
            }
        }

        void render_ocean(entt::entity e) {
            using namespace mirinae::cpnt;

            auto ocean = this->reg().try_get<Ocean>(e);
            if (!ocean)
                return;

            const auto name = fmt::format("Ocean-{}", (ENTT_ID_TYPE)e);

            if (ImGui::CollapsingHeader(name.c_str())) {
                this->render_transform(ocean->transform_);

                ImGui::SliderFloat(
                    "Wind speed",
                    &ocean->wind_speed_,
                    0.0,
                    10000.0,
                    nullptr,
                    ImGuiSliderFlags_Logarithmic
                );
                ImGui::SliderFloat(
                    "Fetch",
                    &ocean->fetch_,
                    0,
                    50000,
                    nullptr,
                    ImGuiSliderFlags_Logarithmic
                );
                ImGui::SliderFloat("Swell", &ocean->swell_, 0, 1);
                ImGui::SliderFloat("Spread blend", &ocean->spread_blend_, 0, 1);

                float wind_dir = sung::to_degrees(
                    std::atan2(ocean->wind_dir_.y, ocean->wind_dir_.x)
                );
                ImGui::SliderFloat("Wind dir", &wind_dir, -179, 179);
                ocean->wind_dir_ = glm::vec2{
                    std::cos(sung::to_radians(wind_dir)),
                    std::sin(sung::to_radians(wind_dir))
                };

                ImGui::DragFloat("Time", &ocean->time_, 0.1);
                if (ImGui::Button("Play"))
                    play_ocean_ = !play_ocean_;

                ImGui::SliderFloat("Tile size", &ocean->tile_size_, 1, 1000);
                ImGui::SliderInt("Tile count X", &ocean->tile_count_x_, 1, 100);
                ImGui::SliderInt("Tile count Y", &ocean->tile_count_y_, 1, 100);
                ImGui::SliderInt("Index", &ocean->idx_, 0, 2);

                for (size_t i = 0; i < Ocean::CASCADE_COUNT; ++i) {
                    ImGui::PushID(i);
                    ImGui::Text("Cascade %d", i);

                    auto& cascade = ocean->cascades_[i];

                    ImGui::Checkbox("Active", &cascade.active_);
                    ImGui::SliderFloat(
                        "Amplitude",
                        &cascade.amplitude_,
                        1000,
                        10000000000,
                        nullptr,
                        ImGuiSliderFlags_Logarithmic
                    );

                    ImGui::SliderFloat("L", &cascade.L_, 1, 100);
                    const auto max_wl = Ocean::max_wavelen(cascade.L_);

                    float low = cascade.cutoff_low_ / max_wl;
                    ImGui::SliderFloat("Cut low", &low, 0, 1);
                    cascade.cutoff_low_ = low * max_wl;

                    float high = cascade.cutoff_high_ / max_wl;
                    ImGui::SliderFloat("Cut high", &high, 0, 1);
                    cascade.cutoff_high_ = high * max_wl;

                    ImGui::SliderFloat2(
                        "TexCo offset", &cascade.texco_offset_[0], -10, 10
                    );
                    ImGui::SliderFloat2(
                        "TexCo scale", &cascade.texco_scale_[0], -10, 10
                    );

                    float magnitude = 1.f / glm::length(cascade.texco_scale_);
                    float phase = std::atan2(
                        cascade.texco_scale_.y, cascade.texco_scale_.x
                    );
                    ImGui::SliderFloat(
                        "TexCo Magnitude",
                        &magnitude,
                        0.01f,
                        10,
                        nullptr,
                        ImGuiSliderFlags_Logarithmic
                    );
                    ImGui::SliderAngle("TexCo Phase", &phase, -179, 179);
                    cascade.texco_scale_ = glm::vec2{
                        std::cos(phase) / magnitude, std::sin(phase) / magnitude
                    };

                    ImGui::PopID();
                }
            }
        }

        void render_atmosphere(entt::entity e) {
            using namespace mirinae::cpnt;

            auto atmosphere = this->reg().try_get<AtmosphereSimple>(e);
            if (!atmosphere)
                return;

            const auto name = fmt::format(
                "AtmosphereSimple-{}", (ENTT_ID_TYPE)e
            );

            if (ImGui::CollapsingHeader(name.c_str())) {
                ImGui::ColorEdit3("Fog color", &atmosphere->fog_color_[0]);
                ImGui::SliderFloat(
                    "Fog density",
                    &atmosphere->fog_density_,
                    0.0,
                    0.01,
                    "%.6f",
                    ImGuiSliderFlags_Logarithmic
                );
            }
        }

        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
        std::array<char, 128> search_buffer_{};
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
                d.set_light_dir(-0.5613, -0.7396, -0.3713);
            }

            // SLight
            {
                flashlight_ = reg.create();
                auto& s = reg.emplace<mirinae::cpnt::SLight>(flashlight_);
                s.transform_.pos_ = { 0, 2, 0 };
                s.color_.set_scaled_color(0, 0, 0);
                s.inner_angle_.set_deg(10);
                s.outer_angle_.set_deg(25);
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
                    const auto l = &reg.emplace<mirinae::cpnt::VPLight>(e);
                    l->pos_ = positions[i];
                    l->color_.set_scaled_color(colors[i]);
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
                ocean.transform_.pos_ = { -30, 8, -30 };
                ocean.wind_speed_ = 300.0;
                ocean.swell_ = 0.3f;
                ocean.spread_blend_ = 0.7f;

                const auto max_wl = ocean.max_wavelen(20);

                ocean.cascades_[0].amplitude_ = 190622176;
                ocean.cascades_[0].cutoff_low_ = 0 * max_wl;
                ocean.cascades_[0].cutoff_high_ = 0.015 * max_wl;
                ocean.cascades_[0].texco_scale_ = { 0.585, 0 };
                ocean.cascades_[0].L_ = 20;

                ocean.cascades_[1].amplitude_ = 132246544;
                ocean.cascades_[1].cutoff_low_ = 0.008 * max_wl;
                ocean.cascades_[1].cutoff_high_ = 0.103 * max_wl;
                ocean.cascades_[1].texco_scale_ = { 1.122, -0.098 };
                ocean.cascades_[1].L_ = 20;

                ocean.cascades_[2].amplitude_ = 107366056;
                ocean.cascades_[2].cutoff_low_ = 0.098 * max_wl;
                ocean.cascades_[2].cutoff_high_ = 1 * max_wl;
                ocean.cascades_[2].texco_scale_ = { 0.886, 0.062 };
                ocean.cascades_[2].L_ = 20;
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
