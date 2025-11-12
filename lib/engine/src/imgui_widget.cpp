#include "mirinae/imgui_widget.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <entt/entity/registry.hpp>
#include <sung/basic/cvar.hpp>

#include "mirinae/cpnt/atmos.hpp"
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
#include "mirinae/lua/script.hpp"


namespace {

    std::vector<std::string> g_texts;


    template <size_t TSize>
    class RollingBuffer {

    public:
        RollingBuffer() { data_.fill(0); }

        void push(float value) {
            data_[index_] = value;
            index_ = (index_ + 1) % TSize;
        }

        const float* data() const { return data_.data(); }
        size_t size() const { return TSize; }

    private:
        std::array<float, TSize> data_;
        size_t index_ = 0;
    };


    struct IWindowDialog : mirinae::imgui::IMainWin {

    public:
        void toggle_show() override { show_ = !show_; }

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
                if (auto c = this->reg().try_get<cpnt::AtmosphereEpic>(e))
                    this->render_cpnt(*c, "Atmosphere Epic");
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
        ImGuiMainWin(
            mirinae::HCosmos cosmos,
            dal::HFilesys filesys,
            std::shared_ptr<mirinae::ScriptEngine> script
        )
            : entt_(cosmos) {
            show_ = false;
            this->set_init_size(680, 640);
            this->add_begin_flag(ImGuiWindowFlags_MenuBar);

            entt_.set_init_size(480, 640);
            entt_.set_init_pos(50, 50);

            cvars_.set_init_size(360, 480);
            cvars_.set_init_pos(50, 50);

            console_.give_script(script);

            const auto font_path = ":asset/font/SeoulNamsanM.ttf";
            if (auto data = filesys->read_file(font_path)) {
                this->add_font(*data);
            }
        }

        void do_frame(const sung::SimClock& clock) override {
            fps_samples_.push(1.0 / clock.dt());
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

                // ImGui::Text("FPS (ImGui): %.1f", ImGui::GetIO().Framerate);
                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                // ImGui::Text("Frame time (ms): %.2f", dt * 1000);

                ImGui::PlotLines(
                    "FPS",
                    fps_samples_.data(),
                    fps_samples_.size(),
                    0,
                    nullptr,
                    0,
                    FLT_MAX
                );

                console_.render_imgui();
                this->end();
            }

            entt_.render();
            cvars_.render();
        }

        void toggle_show() override {
            show_ = !show_;
            if (show_)
                console_.scroll_to_bottom();
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
                    this->enter_text();
                    ImGui::SetKeyboardFocusHere(-1);  // Keep focus on input box
                }

                ImGui::SameLine();
                if (ImGui::Button("Submit")) {
                    this->enter_text();
                }
            }

            void scroll_to_bottom() { scroll_to_bottom_ = true; }

            void give_script(std::shared_ptr<mirinae::ScriptEngine> script) {
                script_ = script;
            }

        private:
            void enter_text() {
                g_texts.push_back(input_buf_.data());
                input_buf_.fill(0);
                scroll_to_bottom_ = true;

                if (script_)
                    script_->exec(g_texts.back().c_str());
            }

            std::shared_ptr<mirinae::ScriptEngine> script_;
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
        ::RollingBuffer<300> fps_samples_;
    };

}  // namespace


namespace mirinae::imgui {

    std::shared_ptr<IMainWin> create_main_win(
        mirinae::HCosmos cosmos,
        dal::HFilesys filesys,
        std::shared_ptr<mirinae::ScriptEngine> script
    ) {
        return std::make_shared<::ImGuiMainWin>(cosmos, filesys, script);
    }

}  // namespace mirinae::imgui
