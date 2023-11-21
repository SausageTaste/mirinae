#pragma once

#include <sung/math/angle.hpp>

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

}
