#pragma once

#include "mirinae/lightweight/time.hpp"
#include "mirinae/scene_2/scene.hpp"


namespace mirinae {

    class CosmosSimulator {

    public:
        void do_frame() { ftime_ = clock_.update(); }

        auto& reg() { return scene_.reg_; }
        auto& ftime() const { return ftime_; }

    private:
        SceneMgr scene_;
        GlobalClock clock_;
        FrameTime ftime_;
    };

}  // namespace mirinae
