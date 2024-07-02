#pragma once

#include "mirinae/scene_2/scene.hpp"


namespace mirinae {

    class CosmosSimulator {

    public:
        auto& reg() { return scene_.reg_; }

    private:
        SceneMgr scene_;
    };

}  // namespace mirinae
