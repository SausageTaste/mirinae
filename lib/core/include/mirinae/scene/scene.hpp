#pragma once

#include <entt/entt.hpp>

#include "mirinae/platform/filesys.hpp"


namespace mirinae { namespace cpnt {

    struct StaticModelActorVk {
        respath_t model_path_;
    };

    struct SkinnedModelActorVk {
        respath_t model_path_;
    };

}}  // namespace mirinae::cpnt


namespace mirinae {

    class Scene {

    public:
        entt::registry reg_;
        std::vector<entt::entity> entt_without_model_;
    };

};  // namespace mirinae
