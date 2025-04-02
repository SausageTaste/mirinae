#pragma once

#include <Jolt/Jolt.h>

#include "mirinae/scene/phys_world.hpp"
#include "mirinae/scene/scene.hpp"


namespace mirinae {

    struct ImGuiRenderUnit {
        virtual ~ImGuiRenderUnit() = default;
        virtual void render() = 0;
    };


    class CosmosSimulator {

    public:
        CosmosSimulator(ScriptEngine& script);

        void tick_clock();
        void do_frame();

        auto& scene() { return scene_; }
        auto& reg() { return *scene_.reg_; }
        auto& reg() const { return scene_.reg_; }
        auto& phys_world() { return phys_world_; }
        auto& clock() const { return clock_; }

        std::vector<std::shared_ptr<ImGuiRenderUnit>> imgui_;

    private:
        Scene scene_;
        PhysWorld phys_world_;
        sung::SimClock clock_;
    };

    using HCosmos = std::shared_ptr<CosmosSimulator>;

}  // namespace mirinae
