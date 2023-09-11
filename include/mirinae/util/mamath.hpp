#pragma once

#include "include_glm.hpp"


namespace mirinae {

    class TransformQuat {

    public:
        glm::mat4 make_model_mat() const {
            const auto rot_mat = glm::mat4_cast(rot_);
            const auto scale_mat = glm::scale(glm::mat4(1.0f), scale_);
            const auto translate_mat = glm::translate(glm::mat4(1.0f), pos_);
            return translate_mat * rot_mat * scale_mat;
        }
        glm::mat4 make_view_mat() const {
            const auto rot_mat = glm::mat4_cast(glm::conjugate(rot_));
            const auto translate_mat = glm::translate(glm::mat4(1.0f), -pos_);
            return rot_mat * translate_mat;
        }

        glm::quat rot_{ 1, 0, 0, 0 };
        glm::vec3 pos_{ 0, 0, 0 };
        glm::vec3 scale_{ 1, 1, 1 };

    };

}
