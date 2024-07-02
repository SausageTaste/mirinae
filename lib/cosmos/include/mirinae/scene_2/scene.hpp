#pragma once

#include <entt/entt.hpp>

#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    struct DLight {
        /**
         * @param view_mat View matrix of camera
         * @return glm::vec3
         */
        glm::vec3 calc_to_light_dir(const glm::dmat4 view_mat) const;

        glm::dmat4 make_proj_mat() const;
        glm::dmat4 make_view_mat() const;
        glm::dmat4 make_light_mat() const;

        TransformQuat<double> transform_;
        glm::vec3 color_;
    };


    struct SLight {
        glm::vec3 calc_view_space_pos(const glm::dmat4 view_mat) const;
        glm::vec3 calc_to_light_dir(const glm::dmat4 view_mat) const;

        glm::dmat4 make_proj_mat() const;
        glm::dmat4 make_view_mat() const;
        glm::dmat4 make_light_mat() const;

        TransformQuat<double> transform_;
        glm::vec3 color_;
        sung::TAngle<double> inner_angle_;
        sung::TAngle<double> outer_angle_;
        double max_distance_ = 100;
    };

}  // namespace mirinae::cpnt


namespace mirinae {

    class SceneMgr {

    public:
        entt::registry reg_;
    };

}  // namespace mirinae
