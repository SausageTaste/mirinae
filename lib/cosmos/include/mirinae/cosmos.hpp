#pragma once

#include "mirinae/scene/scene.hpp"


namespace mirinae {

    class CosmosSimulator {

    public:
        CosmosSimulator(ScriptEngine& script) : scene_(clock_, script) {}

        void tick_clock() { clock_.tick(); }

        void do_frame() { scene_.do_frame(); }

        auto& scene() { return scene_; }
        auto& reg() { return scene_.reg_; }
        auto& reg() const { return scene_.reg_; }
        auto& clock() const { return clock_; }

    private:
        Scene scene_;
        sung::SimClock clock_;
    };

}  // namespace mirinae
