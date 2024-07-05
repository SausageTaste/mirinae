#pragma once

#include <entt/entt.hpp>

#include "mirinae/lightweight/script.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/platform/filesys.hpp"
#include "mirinae/scene/transform.hpp"
#include "mirinae/util/skin_anim.hpp"


namespace mirinae { namespace cpnt {

    struct StaticModelActor {
        respath_t model_path_;
    };

    struct SkinnedModelActor {
        respath_t model_path_;
        SkinAnimState anim_state_;
    };


    struct StandardCamera {
        mirinae::cpnt::Transform view_;
        mirinae::PerspectiveCamera<double> proj_;
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

        void update_time(const FrameTime& ftime) { ftime_ = ftime; }
        const FrameTime& get_time() { return ftime_; }

    public:
        constexpr static uint64_t MAGIC_NUM = 46461236464165;

        entt::registry reg_;
        std::vector<entt::entity> entt_without_model_;
        entt::entity main_camera_ = entt::null;
        const uint64_t magic_num_ = MAGIC_NUM;

    private:
        ScriptEngine& script_;
        FrameTime ftime_;
    };

};  // namespace mirinae
