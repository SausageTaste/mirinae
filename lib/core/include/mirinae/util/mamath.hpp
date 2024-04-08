#pragma once

#include <sung/general/angle.hpp>

#include "include_glm.hpp"


namespace mirinae {

    template <typename T>
    glm::tquat<T> rotate_quat(
        const glm::tquat<T>& q, sung::TAngle<T> angle, const glm::tvec3<T>& axis
    ) {
        return glm::normalize(glm::angleAxis<T>(angle.rad(), axis) * q);
    }


    template <typename T>
    class TransformQuat {

    public:
        using Angle = sung::TAngle<T>;

        void rotate(Angle angle, const glm::tvec3<T>& axis) {
            rot_ = rotate_quat(rot_, angle, axis);
        }

        glm::tmat4x4<T> make_model_mat() const {
            const auto rot_mat = glm::mat4_cast(rot_);
            const auto scale_mat = glm::scale(glm::tmat4x4<T>(1), scale_);
            const auto translate_mat = glm::translate(glm::tmat4x4<T>(1), pos_);
            return translate_mat * rot_mat * scale_mat;
        }

        glm::tmat4x4<T> make_view_mat() const {
            const auto rot_mat = glm::mat4_cast(glm::conjugate(rot_));
            const auto tran_mat = glm::translate(glm::tmat4x4<T>(1), -pos_);
            return rot_mat * tran_mat;
        }

        glm::tquat<T> rot_{ 1, 0, 0, 0 };
        glm::tvec3<T> pos_{ 0, 0, 0 };
        glm::tvec3<T> scale_{ 1, 1, 1 };
    };


    template <typename T>
    class PerspectiveCamera {

    public:
        using Angle = sung::TAngle<T>;

        glm::tmat4x4<T> make_proj_mat(T aspect_ratio) const {
            auto m = glm::perspectiveRH_ZO(
                fov_.rad(), aspect_ratio, near_, far_
            );
            m[1][1] *= -1;
            return m;
        }

        glm::tmat4x4<T> make_proj_mat(T view_width, T view_height) const {
            auto m = glm::perspectiveFovRH_ZO(
                fov_.rad(), view_width, view_height, near_, far_
            );
            m[1][1] *= -1;
            return m;
        }

        Angle fov_ = Angle::from_deg(80);
        T near_ = 0.1;
        T far_ = 1000;
    };

}  // namespace mirinae
