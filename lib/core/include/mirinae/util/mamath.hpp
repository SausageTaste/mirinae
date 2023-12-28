#pragma once

#include <sung/general/angle.hpp>

#include "include_glm.hpp"


namespace mirinae {

    using Angle = sung::TAngle<float>;


    glm::quat rotate_quat(const glm::quat& q, Angle angle, const glm::vec3& axis);


    class TransformQuat {

    public:
        void rotate(Angle angle, const glm::vec3& axis);

        glm::mat4 make_model_mat() const;
        glm::mat4 make_view_mat() const;

        glm::quat rot_{ 1, 0, 0, 0 };
        glm::vec3 pos_{ 0, 0, 0 };
        glm::vec3 scale_{ 1, 1, 1 };

    };


    template <typename T>
    class PerspectiveCamera {

    public:
        glm::tmat4x4<T> make_proj_mat(T aspect_ratio) const {
            auto m = glm::perspective(fov_.rad(), aspect_ratio, near_, far_);
            m[1][1] *= -1;
            return m;
        }

        glm::tmat4x4<T> make_proj_mat(T view_width, T view_height) const {
            return this->make_proj_mat(view_width / view_height);
        }

    public:
        sung::TAngle<T> fov_ = sung::TAngle<T>::from_deg(80);
        T near_ = 0.1;
        T far_ = 1000;

    };

}
