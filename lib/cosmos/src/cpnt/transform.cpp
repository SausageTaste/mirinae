#include "mirinae/cpnt/transform.hpp"

#include <imgui.h>


// Transform
namespace mirinae::cpnt {

    void Transform::render_imgui(const sung::SimClock& clock) {
        const float POS_SPEED = 0.1f;
        const float ROT_SPEED = 0.1f;
        const float SCALE_SPEED = 0.01f;

        ImGui::DragScalarN("Pos", ImGuiDataType_Double, &pos_[0], 3, POS_SPEED);

        auto euler_degrees = glm::degrees(glm::eulerAngles(rot_));
        const auto rot_updated = ImGui::DragScalarN(
            "Rotation (XYZ)",
            ImGuiDataType_Double,
            &euler_degrees[0],
            3,
            ROT_SPEED
        );
        if (rot_updated)
            rot_ = glm::dquat(glm::radians(euler_degrees));

        ImGui::DragScalarN(
            "Scale", ImGuiDataType_Double, &scale_[0], 3, SCALE_SPEED
        );

        if (ImGui::Button("Reset rotation"))
            rot_ = glm::dquat(1, 0, 0, 0);
        if (ImGui::Button("Reset scale"))
            scale_ = glm::dvec3(1);
    }

}  // namespace mirinae::cpnt
