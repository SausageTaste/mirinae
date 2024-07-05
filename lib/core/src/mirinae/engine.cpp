#include "mirinae/engine.hpp"


namespace mirinae {

    class Engine : public IEngine {
    public:
        Engine(EngineCreateInfo&& create_info)
            : create_info_(std::move(create_info)) {}

        ~Engine() override {}

        void do_frame() override {}

        bool is_ongoing() override { return true; }

        void notify_window_resize(uint32_t width, uint32_t height) override {}

    private:
        EngineCreateInfo create_info_;
    };

}  // namespace mirinae


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(
        mirinae::EngineCreateInfo&& create_info
    ) {
        return std::make_unique<Engine>(std::move(create_info));
    }

}  // namespace mirinae
