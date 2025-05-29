#include "mirinae/engine.hpp"

#include <deque>

#include <SDL3/SDL_scancode.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <spdlog/sinks/base_sink.h>
#include <daltools/common/glm_tool.hpp>
#include <daltools/common/task_sys.hpp>
#include <entt/entity/registry.hpp>
#include <sung/basic/cvar.hpp>

#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/envmap.hpp"
#include "mirinae/cpnt/identifier.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/ocean.hpp"
#include "mirinae/cpnt/phys_body.hpp"
#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/network.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/renderer.hpp"


namespace {

    class NoclipController : public mirinae::IInputProcessor {

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
            mirinae::TransformQuat<double>& transform,
            const mirinae::InputActionMapper& action_map,
            const double delta_time
        ) {
            using InputAction = mirinae::InputActionMapper::ActionType;
            using Angle = mirinae::cpnt::Transform::Angle;

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


    struct IWindowDialog : mirinae::ImGuiRenderUnit {

    public:
        bool begin(const char* title) {
            if (!show_)
                return false;

            if (!set_size_once_) {
                set_size_once_ = true;
                ImGui::SetNextWindowPos(init_pos_);
                ImGui::SetNextWindowSize(init_size_);
            }

            const auto t = fmt::format("{}###{}", title, fmt::ptr(this));
            if (ImGui::Begin(t.c_str(), &show_, begin_flags_)) {
                return true;
            } else {
                this->end();
                return false;
            }
        }

        void end() { ImGui::End(); }

        template <typename T>
        void set_init_pos(T x, T y) {
            init_pos_.x = static_cast<float>(x);
            init_pos_.y = static_cast<float>(y);
        }

        template <typename T>
        void set_init_size(T x, T y) {
            init_size_.x = static_cast<float>(x);
            init_size_.y = static_cast<float>(y);
        }

        void add_begin_flag(ImGuiWindowFlags flag) { begin_flags_ |= flag; }

        bool show_ = false;

    private:
        ImVec2 init_pos_{ 10, 10 };
        ImVec2 init_size_{ 300, 200 };
        ImGuiWindowFlags begin_flags_ = 0;
        bool set_size_once_ = false;
    };


    class ImGuiEntt : public IWindowDialog {

    public:
        ImGuiEntt(mirinae::HCosmos& cosmos) : cosmos_(cosmos) {}

        void render() override {
            if (!this->begin("Entities"))
                return;

            for (auto e : this->reg().view<entt::entity>()) {
                this->render_entt(e);
            }

            this->end();
        }

    private:
        const sung::SimClock& clock() const { return cosmos_->scene().clock(); }

        entt::registry& reg() { return cosmos_->reg(); }

        template <typename T, typename... Args>
        void render_cpnt(T& component, const char* name, Args&&... args) {
            constexpr auto flags = ImGuiTreeNodeFlags_DefaultOpen;
            const auto h_name = fmt::format(
                "{}###{}", name, fmt::ptr(&component)
            );

            ImGui::Indent(10);
            if (ImGui::CollapsingHeader(h_name.c_str(), flags)) {
                ImGui::Indent(10);
                ImGui::PushID(&component);
                component.render_imgui(std::forward<Args>(args)...);
                ImGui::PopID();
                ImGui::Unindent(10);
            }
            ImGui::Unindent(10);
        }

        void render_entt(entt::entity e) {
            namespace cpnt = mirinae::cpnt;

            auto entt_name = fmt::format("[{}]", (ENTT_ID_TYPE)e);
            if (auto c = this->reg().try_get<cpnt::Id>(e)) {
                entt_name += ' ';
                entt_name += c->name_.data();
            }

            if (ImGui::CollapsingHeader(entt_name.c_str())) {
                if (auto c = this->reg().try_get<cpnt::Id>(e))
                    this->render_cpnt(*c, "ID");
                if (auto c = this->reg().try_get<cpnt::StandardCamera>(e))
                    this->render_cpnt(*c, "Standard Camera");
                if (auto c = this->reg().try_get<cpnt::MdlActorStatic>(e))
                    this->render_cpnt(*c, "Static Actor");
                if (auto c = this->reg().try_get<cpnt::MdlActorSkinned>(e))
                    this->render_cpnt(*c, "Skinned Actor", this->clock());
                if (auto c = this->reg().try_get<cpnt::DLight>(e))
                    this->render_cpnt(*c, "Directional Light");
                if (auto c = this->reg().try_get<cpnt::SLight>(e))
                    this->render_cpnt(*c, "Spotlight");
                if (auto c = this->reg().try_get<cpnt::VPLight>(e))
                    this->render_cpnt(*c, "Volumetric Point Light");
                if (auto c = this->reg().try_get<cpnt::Terrain>(e))
                    this->render_cpnt(*c, "Terrain");
                if (auto c = this->reg().try_get<cpnt::Ocean>(e))
                    this->render_cpnt(*c, "Ocean");
                if (auto c = this->reg().try_get<cpnt::AtmosphereSimple>(e))
                    this->render_cpnt(*c, "Atmosphere Simple");
                if (auto c = this->reg().try_get<cpnt::Envmap>(e))
                    this->render_cpnt(*c, "Envmap");
                if (auto c = this->reg().try_get<cpnt::Transform>(e))
                    this->render_cpnt(*c, "Transform");
            }
        }

        mirinae::HCosmos cosmos_;
    };


    class ImGuiCvars : public IWindowDialog {

    public:
        void render() override {
            if (!this->begin("CVars"))
                return;

            CvarVisitor visitor{};
            sung::gcvars().visit(visitor);

            this->end();
        }

    private:
        class CvarVisitor : public sung::ICVarVisitor {

        public:
            void visit(sung::ICVarInt& cvar) override {
                int64_t v = cvar.get();
                if (ImGui::DragScalar(cvar.id().c_str(), ImGuiDataType_S64, &v))
                    cvar.set(v);
                if (this->need_tooltip(cvar))
                    ImGui::SetTooltip("%s", cvar.help().c_str());
            }

            void visit(sung::ICVarFloat& cvar) override {
                double v = cvar.get();
                const auto name = cvar.id().c_str();
                if (ImGui::DragScalar(name, ImGuiDataType_Double, &v, 0.1f))
                    cvar.set(v);
                if (this->need_tooltip(cvar))
                    ImGui::SetTooltip("%s", cvar.help().c_str());
            }

            void visit(sung::ICVarStr& cvar) override {
                auto str = cvar.get();
                if (ImGui::InputText(cvar.id().c_str(), &str))
                    cvar.set(str);
                if (this->need_tooltip(cvar))
                    ImGui::SetTooltip("%s", cvar.help().c_str());
            }

        private:
            static bool need_tooltip(sung::ICVarValue& cvar) {
                if (cvar.help().empty())
                    return false;
                return ImGui::IsItemHovered(
                    ImGuiHoveredFlags_AllowWhenDisabled
                );
            }
        };

        void render_cvar() {
            CvarVisitor v{};
            sung::gcvars().visit(v);
        }
    };


    class ImGuiMainWin : public IWindowDialog {

    public:
        ImGuiMainWin(mirinae::HCosmos& cosmos, dal::HFilesys& filesys)
            : entt_(cosmos) {
            show_ = false;
            this->set_init_size(680, 640);
            this->add_begin_flag(ImGuiWindowFlags_MenuBar);

            entt_.set_init_size(480, 640);
            entt_.set_init_pos(50, 50);

            cvars_.set_init_size(360, 480);
            cvars_.set_init_pos(50, 50);

            const auto font_path = ":asset/font/SeoulNamsanM.ttf";
            if (auto data = filesys->read_file(font_path)) {
                this->add_font(*data);
            }
        }

        void render() override {
            if (this->begin("Main Window")) {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("View")) {
                        ImGui::MenuItem("Entities", nullptr, &entt_.show_);
                        ImGui::MenuItem("CVars", nullptr, &cvars_.show_);
                        ImGui::EndMenu();
                    }

                    ImGui::EndMenuBar();
                }

                ImGui::Text("FPS (ImGui): %.1f", ImGui::GetIO().Framerate);
                console_.render_imgui();

                this->end();
            }

            entt_.render();
            cvars_.render();
        }

    private:
        class DevConsole {

        public:
            void render_imgui() {
                if (ImGui::BeginChild(
                        "ScrollingRegion",
                        ImVec2(0, -30),
                        true,
                        ImGuiWindowFlags_HorizontalScrollbar
                    )) {
                    ImGuiListClipper clipper;
                    clipper.Begin(g_texts.size());
                    while (clipper.Step())
                        for (int i = clipper.DisplayStart;
                             i < clipper.DisplayEnd;
                             i++) {
                            ImGui::TextUnformatted(g_texts[i].c_str());
                        }

                    if (scroll_to_bottom_)
                        ImGui::SetScrollHereY(1.0f);
                    scroll_to_bottom_ = false;
                }

                ImGui::EndChild();
                if (ImGui::InputText(
                        "##input",
                        input_buf_.data(),
                        input_buf_.size(),
                        ImGuiInputTextFlags_EnterReturnsTrue
                    )) {
                    g_texts.push_back(input_buf_.data());
                    input_buf_.fill(0);
                    scroll_to_bottom_ = true;
                    ImGui::SetKeyboardFocusHere(-1);  // Keep focus on input box
                }

                ImGui::SameLine();
                if (ImGui::Button("Submit")) {
                    g_texts.push_back(input_buf_.data());
                    input_buf_.fill(0);
                }
            }

            void scroll_to_bottom() { scroll_to_bottom_ = true; }

        private:
            std::array<char, 256> input_buf_{};
            bool scroll_to_bottom_ = false;
        };

        void add_font(const std::vector<sung::byte8>& data) {
            const int arr_size = data.size();
            const auto arr = new sung::byte8[arr_size];
            std::copy(data.begin(), data.end(), arr);

            auto& io = ImGui::GetIO();
            auto& fonts = *io.Fonts;
            fonts.Clear();

            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
            // builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
            builder.BuildRanges(&ranges_);

            fonts.AddFontFromMemoryTTF(arr, arr_size, 20, 0, ranges_.Data);
        }

        DevConsole console_;

        ImGuiEntt entt_;
        ImGuiCvars cvars_;

        ImVector<ImWchar> ranges_;
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

            auto& reg = cosmos_->reg();

            // Physice object
            for (int x = 0; x < 0; ++x) {
                for (int y = 0; y < 5; ++y) {
                    for (int z = 0; z < 2; ++z) {
                        const auto entt = reg.create();

                        auto& id = reg.emplace<mirinae::cpnt::Id>(entt);
                        id.set_name("physics object");

                        auto& mdl = reg.emplace<mirinae::cpnt::MdlActorStatic>(
                            entt
                        );
                        mdl.model_path_ = "Sung/sphere.dun/sphere.dmd";

                        auto& tform = reg.emplace<mirinae::cpnt::Transform>(
                            entt
                        );
                        const auto radius = 1.0;
                        const auto jitter = (x + y + z) * radius * 0.2;
                        tform.pos_ = glm::dvec3{
                            2.2 * radius * x - 50 - jitter,
                            2.2 * radius * y + 20,
                            2.2 * radius * z + 00 + jitter,
                        };
                        tform.set_scale(radius);

                        cosmos_->phys_world().give_body(entt, reg);
                    }
                }
            }
            cosmos_->phys_world().optimize();

            // DLight
            {
                constexpr auto d45 = mirinae::cpnt::Transform::Angle::from_deg(
                    45
                );

                const auto entt = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("Sun Light");

                auto& d = reg.emplace<mirinae::cpnt::DLight>(entt);
                d.color_.set_scaled_color(5);

                auto& t = reg.emplace<mirinae::cpnt::Transform>(entt);
                t.reset_rotation();
                t.rotate(-d45, glm::vec3{ 1, 0, 0 });
                t.rotate(d45, glm::vec3{ 0, 1, 0 });
            }

            // DLight
            /*
            {
                const auto entt = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("Sun Light 2");

                auto& d = reg.emplace<mirinae::cpnt::DLight>(entt);
                d.color_.set_scaled_color(0, 0, 5);

                auto& t = reg.emplace<mirinae::cpnt::Transform>(entt);
                d.set_light_dir(0.5613, -0.7396, -0.3713, t);
            }
            */

            // SLight
            /*
            {
                flashlight_ = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(flashlight_);
                i.set_name("Flashlight");

                auto& s = reg.emplace<mirinae::cpnt::SLight>(flashlight_);
                s.color_.set_scaled_color(0, 0, 0);
                s.inner_angle_.set_deg(10);
                s.outer_angle_.set_deg(25);

                auto& t = reg.emplace<mirinae::cpnt::Transform>(flashlight_);
                t.pos_ = { 0, 2, 0 };
            }

            // SLight
            {
                const auto e = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(e);
                i.set_name("Spotlight 1");

                auto& s = reg.emplace<mirinae::cpnt::SLight>(e);
                s.color_.set_scaled_color(10);
                s.inner_angle_.set_deg(10);
                s.outer_angle_.set_deg(25);

                auto& t = reg.emplace<mirinae::cpnt::Transform>(e);
                t.pos_ = { -106.85, 7.77, -51.13 };
                t.rot_ = glm::normalize(
                    glm::dquat{
                        0.202623,
                        -0.444979,
                        0.855584,
                        0.170041,
                    }
                );
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

                    auto& id = reg.emplace<mirinae::cpnt::Id>(e);
                    id.set_name(fmt::format("VPLight-{}", i).c_str());

                    auto& l = reg.emplace<mirinae::cpnt::VPLight>(e);
                    l.color_.set_scaled_color(colors[i]);

                    auto& t = reg.emplace<mirinae::cpnt::Transform>(e);
                    t.pos_ = positions[i];
                }
            }
            */

            // Main Camera
            {
                const auto entt = reg.create();
                cosmos_->scene().main_camera_ = entt;
                cosmos_->cam_ctrl().set_camera(entt);

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("Main Camera");

                auto& cam = reg.emplace<mirinae::cpnt::StandardCamera>(entt);
                cam.proj_.near_ = 0.1;
                cam.proj_.far_ = 100000.000;
                cam.exposure_ = 10;

                auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
                tform.pos_ = {
                    -114.50,
                    6.89,
                    -45.62,
                };
                tform.rot_ = glm::normalize(
                    glm::dquat{
                        -0.376569,
                        0.056528,
                        0.914417,
                        0.137265,
                    }
                );
            }

            // Envmap
            {
                const auto e = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(e);
                i.set_name("Main envmap");

                auto& envmap = reg.emplace<mirinae::cpnt::Envmap>(e);

                auto& tform = reg.emplace<mirinae::cpnt::Transform>(e);
                tform.pos_ = { -99.15, 4.98, -25.26 };
            }

            // Ocean
            /*
            {
                const auto entt = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("Ocean");

                auto& ocean = reg.emplace<mirinae::cpnt::Ocean>(entt);
                ocean.height_ = -50;
                ocean.wind_speed_ = 1;
                ocean.fetch_ = 100000;
                ocean.spread_blend_ = 0.284;
                ocean.swell_ = 0.284;
                ocean.depth_ = 500;
                ocean.roughness_ = 0.01;

                constexpr double len_scale0 = 250;
                constexpr double len_scale1 = 17;
                constexpr double len_scale2 = 5;
                constexpr auto boundary1 = SUNG_TAU / len_scale1 * 6;
                constexpr auto boundary2 = SUNG_TAU / len_scale2 * 6;

                auto cas = &ocean.cascades_[0];
                cas->lod_scale_ = len_scale0;
                cas->cutoff_low_ = 0.0001;
                cas->cutoff_high_ = boundary1;
                cas->amplitude_ = 1;
                cas->L_ = cas->lod_scale_;

                cas = &ocean.cascades_[1];
                cas->lod_scale_ = len_scale1;
                cas->cutoff_low_ = boundary1;
                cas->cutoff_high_ = boundary2;
                cas->amplitude_ = 1;
                cas->L_ = cas->lod_scale_;

                cas = &ocean.cascades_[2];
                cas->lod_scale_ = len_scale2;
                cas->cutoff_low_ = boundary2;
                cas->cutoff_high_ = 9999;
                cas->amplitude_ = 0.3;
                cas->L_ = cas->lod_scale_;
            }
            */

            // Atmosphere
            {
                const auto entt = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("Atmosphere Simple");

                auto& atm = reg.emplace<mirinae::cpnt::AtmosphereSimple>(entt);
                atm.fog_color_ = { 0.556, 0.707, 0.846 };
                atm.sky_tex_path_ = ":asset/textures/empty_sky.hdr";
                atm.mie_anisotropy_ = 0.8;
            }

            // https://manticorp.github.io/unrealheightmap/#latitude/36.271/longitude/-112.357/zoom/14/outputzoom/14/width/4096/height/4096
            // https://manticorp.github.io/unrealheightmap/#latitude/46.453/longitude/10.635/zoom/12/outputzoom/12/width/8129/height/8129

            // Terrain
            {
                const auto entt = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("Terrain");

                auto& terrain = reg.emplace<mirinae::cpnt::Terrain>(entt);
                terrain.height_map_path_ =
                    "Sung/36_271_-112_357_14_4096_4096.png";
                terrain.albedo_map_path_ =
                    "Sung/36_271_-112_357_14_4096_4096_albedo_imagery.png";
                terrain.terrain_width_ = 31500;
                terrain.terrain_height_ = 31500;
                terrain.height_scale_ = 2761.32 - 585.625;
                terrain.tile_count_x_ = 20;
                terrain.tile_count_y_ = 20;

                auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
                tform.pos_ = { -513, 0, 49 };

                cosmos_->phys_world().give_body_height_field(entt, reg);
            }

            // Player model
            {
                const auto entt = reg.create();
                cosmos_->cam_ctrl().set_target(entt);

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("Player model");

                auto& mdl = reg.emplace<mirinae::cpnt::MdlActorSkinned>(entt);

                auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
                tform.pos_ = { 0, 1290, 0 };

#if true
                cosmos_->cam_ctrl().anim_idle_ = "idle_normal_1";
                cosmos_->cam_ctrl().anim_walk_ = "evt1_walk_normal_1";
                cosmos_->cam_ctrl().anim_run_ = "run_normal_1";
                cosmos_->cam_ctrl().anim_sprint_ = "hwan_run_battle_1";
                cosmos_->cam_ctrl().player_model_heading_.set_zero();
                mdl.model_path_ = "Sung/artist.dun/artist_subset.dmd";
                mdl.anim_state_.select_anim_name(
                    "idle_normal_1", cosmos_->scene().clock()
                );
#else
                cosmos_->cam_ctrl().anim_idle_ = "standing";
                cosmos_->cam_ctrl().anim_walk_ = "run";
                cosmos_->cam_ctrl().anim_run_ = "run";
                cosmos_->cam_ctrl().anim_sprint_ = "run";
                cosmos_->cam_ctrl().player_model_heading_.set_deg(90);
                mdl.model_path_ = "Sung/adobe_bald_guy.dun/adobe_bald_guy.dmd";
                mdl.anim_state_.select_anim_index(0, cosmos_->scene().clock());
                tform.set_scale(0.007);
#endif

                auto& phys = reg.emplace<mirinae::cpnt::CharacterPhys>(entt);
                phys.height_ = 1;
                phys.radius_ = 0.15;
            }

            // City
            {
                const auto entt = reg.create();

                auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
                i.set_name("City");

                auto& mdl = reg.emplace<mirinae::cpnt::MdlActorStatic>(entt);
                mdl.model_path_ = "Sung/city.dun/city.dmd";

                auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
                tform.pos_ = { 0, 1289, 0 };
                tform.set_scale(0.65);

                cosmos_->phys_world().give_body_triangles(entt, reg);
            }

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
                imgui_main_ = std::make_shared<ImGuiMainWin>(
                    cosmos_, ecinfo_.filesys_
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
            if (e.scancode_ == SDL_SCANCODE_F) {
                auto& reg = cosmos_->reg();
                auto& slight = reg.get<mirinae::cpnt::SLight>(flashlight_);
                if (e.action_type == mirinae::key::ActionType::down) {
                    if (slight.color_.intensity() == 0)
                        slight.color_.set_scaled_color(5, 5, 5);
                    else
                        slight.color_.intensity() = 0;
                }
                return true;
            } else if (e.scancode_ == SDL_SCANCODE_GRAVE) {
                if (e.action_type == mirinae::key::ActionType::up)
                    imgui_main_->show_ = !imgui_main_->show_;
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
            namespace cpnt = mirinae::cpnt;

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
            namespace cpnt = mirinae::cpnt;

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
        std::shared_ptr<::ImGuiMainWin> imgui_main_;
        std::unique_ptr<mirinae::IRenderer> renderer_;

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
