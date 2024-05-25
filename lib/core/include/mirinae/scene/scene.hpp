#pragma once

#include <entt/entt.hpp>

#include "mirinae/platform/filesys.hpp"
#include "mirinae/util/script.hpp"
#include "mirinae/util/skin_anim.hpp"


namespace mirinae { namespace cpnt {

    struct StaticModelActor {
        respath_t model_path_;
    };

    struct SkinnedModelActor {
        respath_t model_path_;
        SkinAnimState anim_state_;
    };

}}  // namespace mirinae::cpnt


namespace mirinae {

    class Scene {

    public:
        Scene(ScriptEngine& script);

        Scene(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene& operator=(Scene&&) = delete;

    public:
        constexpr static uint64_t MAGIC_NUM = 46461236464165;

        entt::registry reg_;
        std::vector<entt::entity> entt_without_model_;
        const uint64_t magic_num_ = MAGIC_NUM;

    private:
        ScriptEngine& script_;
    };

};  // namespace mirinae
