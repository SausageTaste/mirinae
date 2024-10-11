#pragma once

#include <entt/entt.hpp>

#include "mirinae/lightweight/script.hpp"
#include "mirinae/lightweight/skin_anim.hpp"
#include "mirinae/lightweight/time.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/platform/filesys.hpp"


namespace mirinae::cpnt {

    using Transform = TransformQuat<double>;


    struct DLight {
        /**
         * @param view_mat View matrix of camera
         * @return glm::vec3
         */
        glm::dvec3 calc_to_light_dir(const glm::dmat4 view_mat) const;

        // glm::dmat4 make_proj_mat() const;
        // glm::dmat4 make_view_mat() const;
        // glm::dmat4 make_light_mat() const;
        glm::dmat4 make_light_mat(const std::array<glm::dvec3, 8>& p) const;

        // Set the direction of the light ray traveling out of the light source
        void set_light_dir(glm::dvec3 dir);
        void set_light_dir(double x, double y, double z) {
            this->set_light_dir(glm::dvec3{ x, y, z });
        }

        TransformQuat<double> transform_;
        glm::vec3 color_;
    };


    struct SLight {
        glm::dvec3 calc_view_space_pos(const glm::dmat4 view_mat) const;
        glm::dvec3 calc_to_light_dir(const glm::dmat4 view_mat) const;

        glm::dmat4 make_proj_mat() const;
        glm::dmat4 make_view_mat() const;
        glm::dmat4 make_light_mat() const;

        TransformQuat<double> transform_;
        glm::vec3 color_;
        sung::TAngle<double> inner_angle_;
        sung::TAngle<double> outer_angle_;
        double max_distance_ = 100;
    };

    struct StaticModelActor {
        respath_t model_path_;
    };

    struct SkinnedModelActor {
        respath_t model_path_;
        SkinAnimState anim_state_;
    };


    struct StandardCamera {
        TransformQuat<double> view_;
        PerspectiveCamera<double> proj_;
    };

}  // namespace mirinae::cpnt


namespace mirinae {

    class Scene {

    public:
        Scene(ScriptEngine& script);

        auto& ftime() const { return ftime_; }

    public:
        constexpr static uint64_t MAGIC_NUM = 46461236464165;

        entt::registry reg_;
        std::vector<entt::entity> entt_without_model_;
        entt::entity main_camera_ = entt::null;
        FrameTime ftime_;
        const uint64_t magic_num_ = MAGIC_NUM;

    private:
        ScriptEngine& script_;
    };

}  // namespace mirinae
