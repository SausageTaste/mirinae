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
        CosmosSimulator(ScriptEngine& script) : scene_(clock_, script) {}

        void tick_clock() { clock_.tick(); }

        void do_frame() {
            scene_.do_frame();
            phys_world_.pre_sync(scene_.clock().dt(), *scene_.reg_);
            phys_world_.do_frame(scene_.clock().dt());
            phys_world_.post_sync(scene_.clock().dt(), *scene_.reg_);
        }

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
