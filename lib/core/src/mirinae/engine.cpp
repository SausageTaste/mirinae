#include "mirinae/engine.hpp"

#include "mirinae/renderer.hpp"


namespace {

    class Engine : public mirinae::IEngine {

    public:
        Engine(mirinae::EngineCreateInfo&& cinfo) {
            auto filesys = cinfo.filesys_;

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
            cosmos_->do_frame();
            renderer_->do_frame();
        }

        bool is_ongoing() override { return true; }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            renderer_->notify_window_resize(width, height);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            return renderer_->on_key_event(e);
        }

        bool on_text_event(char32_t c) override {
            return renderer_->on_text_event(c);
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            return renderer_->on_mouse_event(e);
        }

    private:
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
        std::unique_ptr<mirinae::IRenderer> renderer_;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(EngineCreateInfo&& cinfo) {
        return std::make_unique<::Engine>(std::move(cinfo));
    }

}  // namespace mirinae
