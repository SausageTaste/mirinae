#pragma once

#include "mirinae/scene/scene.hpp"


namespace mirinae {

    class CosmosSimulator {

    public:
        CosmosSimulator(ScriptEngine& script)
            : scene_(script) {}

        void do_frame() { scene_.ftime_ = clock_.update(); }

        auto& scene() { return scene_; }
        auto& reg() { return scene_.reg_; }
        auto& reg() const { return scene_.reg_; }
        auto& ftime() const { return scene_.ftime_; }

    private:
        Scene scene_;
        GlobalClock clock_;
    };

}  // namespace mirinae
