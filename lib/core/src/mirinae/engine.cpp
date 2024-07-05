#include "mirinae/engine.hpp"

#include "mirinae/renderer.hpp"


namespace {

    class Engine : public mirinae::IEngine {

    public:
        Engine(mirinae::EngineCreateInfo&& cinfo) {
            renderer_ = mirinae::create_vk_renderer(std::move(cinfo));
        }

        ~Engine() override {}

        void do_frame() override { renderer_->do_frame(); }

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
        std::unique_ptr<mirinae::IRenderer> renderer_;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(EngineCreateInfo&& cinfo) {
        return std::make_unique<::Engine>(std::move(cinfo));
    }

}  // namespace mirinae
