#include "mirinae/scene_2/scene.hpp"


// DLight
namespace mirinae::cpnt {

    glm::vec3 DLight::calc_to_light_dir(const glm::dmat4 view_mat) const {
        const auto v = view_mat * transform_.make_model_mat() *
                       glm::dvec4(0, 0, 1, 0);
        return glm::normalize(glm::vec3(v));
    };

    glm::dmat4 DLight::make_proj_mat() const {
        auto p = glm::orthoRH_ZO<double>(-10, 10, -10, 10, -50, 50);
        return p;
    }

    glm::dmat4 DLight::make_view_mat() const {
        return transform_.make_view_mat();
    }

    glm::dmat4 DLight::make_light_mat() const {
        return make_proj_mat() * make_view_mat();
    }

}  // namespace mirinae::cpnt


// SLight
namespace mirinae::cpnt {

    glm::vec3 SLight::calc_view_space_pos(const glm::dmat4 view_mat) const {
        const auto v = view_mat * glm::dvec4(transform_.pos_, 1);
        return glm::vec3(v);
    }

    glm::vec3 SLight::calc_to_light_dir(const glm::dmat4 view_mat) const {
        const auto v = view_mat * transform_.make_model_mat() *
                       glm::dvec4(0, 0, 1, 0);
        return glm::normalize(glm::vec3(v));
    };

    glm::dmat4 SLight::make_proj_mat() const {
        return glm::perspectiveRH_ZO(
            outer_angle_.rad() * 2, 1.0, 0.1, max_distance_
        );
    }

    glm::dmat4 SLight::make_view_mat() const {
        return transform_.make_view_mat();
    }

    glm::dmat4 SLight::make_light_mat() const {
        return make_proj_mat() * make_view_mat();
    }

}  // namespace mirinae::cpnt
