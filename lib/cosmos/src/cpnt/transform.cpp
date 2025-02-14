#include "mirinae/cpnt/transform.hpp"

#include <imgui.h>


// Transform
namespace mirinae::cpnt {

    void Transform::render_imgui(const sung::SimClock& clock) {
        auto transformf = this->copy<float>();
        glm::vec3 rot{ 0 };

        ImGui::DragFloat3("Pos", &transformf.pos_[0]);
        ImGui::DragFloat3("Rot", &rot[0]);
        if (ImGui::Button("Reset rotation"))
            transformf.rot_ = glm::quat(1, 0, 0, 0);
        ImGui::DragFloat3("Scale", &transformf.scale_[0]);

        this->TransformQuat<double>::operator=(transformf.copy<double>());
        this->rotate(Angle::from_deg(rot.x), glm::vec3{ 1, 0, 0 });
        this->rotate(Angle::from_deg(rot.y), glm::vec3{ 0, 1, 0 });
        this->rotate(Angle::from_deg(rot.z), glm::vec3{ 0, 0, 1 });
    }

}  // namespace mirinae::cpnt
