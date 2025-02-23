#include "mirinae/cpnt/transform.hpp"

#include <imgui.h>


// Transform
namespace mirinae::cpnt {

    void Transform::render_imgui() {
        const float POS_SPEED = 0.1f;
        const float ROT_SPEED = 0.1f;
        const float SCALE_SPEED = 0.01f;

        ImGui::DragScalarN(
            "Pos", ImGuiDataType_Double, &pos_[0], 3, POS_SPEED, 0, 0, "%.2f"
        );

        ImGui::Separator();

        {
            ImGui::DragScalarN(
                "Quat (XYZW)", ImGuiDataType_Double, &rot_[0], 4, ROT_SPEED
            );

            glm::dvec3 ypr{ 0, 0, 0 };
            const auto yrp_update = ImGui::DragScalarN(
                "Yaw Pitch Roll",
                ImGuiDataType_Double,
                &ypr[0],
                3,
                ROT_SPEED,
                0,
                0,
                0

            );
            if (yrp_update) {
                this->rotate(Angle::from_deg(ypr[0]), this->make_up_dir());
                this->rotate(Angle::from_deg(-ypr[1]), this->make_right_dir());
                this->rotate(
                    Angle::from_deg(-ypr[2]), this->make_forward_dir()
                );
            }

            if (ImGui::Button("Reset rotation"))
                rot_ = glm::dquat(1, 0, 0, 0);
        }

        ImGui::Separator();

        {
            ImGui::DragScalarN(
                "Scale", ImGuiDataType_Double, &scale_[0], 3, SCALE_SPEED
            );

            double scale = (scale_.x + scale_.y + scale_.z) / 3.0;
            const auto uni_scale_udpate = ImGui::DragScalar(
                "Uniform scale", ImGuiDataType_Double, &scale, SCALE_SPEED
            );
            if (uni_scale_udpate)
                scale_ = glm::dvec3(scale);

            if (ImGui::Button("Reset scale"))
                scale_ = glm::dvec3(1);
        }
    }

}  // namespace mirinae::cpnt
