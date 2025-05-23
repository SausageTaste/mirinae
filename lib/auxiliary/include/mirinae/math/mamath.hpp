#pragma once

#include <sung/basic/angle.hpp>

#include "mirinae/math/include_glm.hpp"


namespace mirinae {

    template <typename T>
    glm::tvec2<T> rotate_vec(const glm::tvec2<T>& vec, sung::TAngle<T> angle) {
        const auto cos_a = std::cos(angle.rad());
        const auto sin_a = std::sin(angle.rad());
        return glm::tvec2<T>{ vec.x * cos_a - vec.y * sin_a,
                              vec.x * sin_a + vec.y * cos_a };
    }

    template <typename T>
    glm::tquat<T> rotate_quat(
        const glm::tquat<T>& q, sung::TAngle<T> angle, const glm::tvec3<T>& axis
    ) {
        return glm::normalize(glm::angleAxis<T>(angle.rad(), axis) * q);
    }

    template <typename T>
    glm::tmat4x4<T> make_perspective(
        sung::TAngle<T> fovy, T aspect, T zNear, T zFar
    ) {
        auto m = glm::perspectiveRH_ZO(fovy.rad(), aspect, zFar, zNear);
        m[1][1] *= -1;
        return m;
    }

    template <typename T>
    glm::tmat4x4<T> make_perspective(
        sung::TAngle<T> fovy, T width, T height, T zNear, T zFar
    ) {
        auto m = glm::perspectiveFovRH_ZO(
            fovy.rad(), width, height, zFar, zNear
        );
        m[1][1] *= -1;
        return m;
    }


    template <typename T>
    class TransformQuat {

    public:
        using Angle = sung::TAngle<T>;

        void rotate(Angle angle, const glm::tvec3<T>& axis) {
            rot_ = rotate_quat(rot_, angle, axis);
        }
        void reset_rotation() { rot_ = glm::quat(1, 0, 0, 0); }

        void set_scale(T x) {
            scale_.x = x;
            scale_.y = x;
            scale_.z = x;
        }

        glm::tvec3<T> make_forward_dir() const {
            return glm::normalize(
                glm::mat3_cast(rot_) * glm::tvec3<T>(0, 0, -1)
            );
        }
        glm::tvec3<T> make_up_dir() const {
            return glm::normalize(
                glm::mat3_cast(rot_) * glm::tvec3<T>(0, 1, 0)
            );
        }
        glm::tvec3<T> make_right_dir() const {
            return glm::normalize(
                glm::mat3_cast(rot_) * glm::tvec3<T>(1, 0, 0)
            );
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

        template <typename U>
        TransformQuat<U> copy() const {
            TransformQuat<U> ret;
            ret.rot_ = glm::tquat<U>{ rot_ };
            ret.pos_ = glm::tvec3<U>{ pos_ };
            ret.scale_ = glm::tvec3<U>{ scale_ };
            return ret;
        }

        glm::tquat<T> rot_{ 1, 0, 0, 0 };
        glm::tvec3<T> pos_{ 0, 0, 0 };
        glm::tvec3<T> scale_{ 1, 1, 1 };
    };


    template <typename T>
    class PerspectiveCamera {

    public:
        using Angle = sung::TAngle<T>;

        void multiply_fov(T factor) {
            fov_.set_rad(
                sung::clamp<T>(fov_.rad() * factor, 0.001, SUNG_PI - 0.001)
            );
        }

        glm::tmat4x4<T> make_proj_mat(T aspect_ratio) const {
            return make_perspective(fov_, aspect_ratio, near_, far_);
        }

        glm::tmat4x4<T> make_proj_mat(T view_width, T view_height) const {
            return make_perspective(
                fov_,
                std::max<T>(view_width, 1),
                std::max<T>(view_height, 1),
                near_,
                far_
            );
        }

        Angle fov_ = Angle::from_deg(60);
        T near_ = 0.01;
        T far_ = 1000;
    };

}  // namespace mirinae
