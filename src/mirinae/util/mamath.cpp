#include "mirinae/util/mamath.hpp"


namespace mirinae {

    glm::quat rotate_quat(const glm::quat& q, const float angle, const glm::vec3& axis) {
        return glm::normalize(glm::angleAxis(angle, axis) * q);
    }

}


// TransformQuat
namespace mirinae {

    void TransformQuat::rotate(const float angle, const glm::vec3& axis) {
        rot_ = rotate_quat(rot_, angle, axis);
    }

    glm::mat4 TransformQuat::make_model_mat() const {
        const auto rot_mat = glm::mat4_cast(rot_);
        const auto scale_mat = glm::scale(glm::mat4(1.0f), scale_);
        const auto translate_mat = glm::translate(glm::mat4(1.0f), pos_);
        return translate_mat * rot_mat * scale_mat;
    }

    glm::mat4 TransformQuat::make_view_mat() const {
        const auto rot_mat = glm::mat4_cast(glm::conjugate(rot_));
        const auto translate_mat = glm::translate(glm::mat4(1.0f), -pos_);
        return rot_mat * translate_mat;
    }

}
