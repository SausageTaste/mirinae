#pragma once

#include <entt/entt.hpp>

#include "mirinae/platform/filesys.hpp"
#include "mirinae/scene/transform.hpp"
#include "mirinae/util/mamath.hpp"
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


    struct DLight {
        /**
         * @param view_mat View matrix of camera
         * @return glm::vec3
         */
        glm::vec3 calc_to_light_dir(const glm::dmat4 view_mat) const {
            const auto v = view_mat * transform_.make_model_mat() *
                           glm::dvec4(0, 0, 1, 0);
            return glm::normalize(glm::vec3(v));
        };

        glm::dmat4 make_proj_mat() const {
            auto p = glm::ortho<double>(-50, 50, -50, 50, -100, 100);
            p[1][1] *= -1;
            return p;
        }
        glm::dmat4 make_view_mat() const { return transform_.make_view_mat(); }
        glm::dmat4 make_light_mat() const {
            return make_proj_mat() * make_view_mat();
        }

        Transform transform_;
        glm::vec3 color_;
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

        void update_time() { ftime_ = global_clock_.update(); }
        const FrameTime& get_time() { return ftime_; }

    public:
        constexpr static uint64_t MAGIC_NUM = 46461236464165;

        entt::registry reg_;
        std::vector<entt::entity> entt_without_model_;
        entt::entity main_camera_ = entt::null;
        const uint64_t magic_num_ = MAGIC_NUM;

    private:
        ScriptEngine& script_;
        GlobalClock global_clock_;
        FrameTime ftime_;
    };

};  // namespace mirinae
