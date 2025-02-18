#include "mirinae/cpnt/camera.hpp"

#include <imgui.h>


namespace mirinae::cpnt {

    void StandardCamera::render_imgui(const sung::SimClock& clock) {
        float angle = (float)proj_.fov_.deg();
        ImGui::SliderFloat(
            "FOV", &angle, 0.01f, 179.99f, nullptr, ImGuiSliderFlags_Logarithmic
        );
        proj_.fov_.set_deg(angle);

        float near = (float)proj_.near_;
        ImGui::SliderFloat(
            "Near", &near, 0.001f, 30000, nullptr, ImGuiSliderFlags_Logarithmic
        );
        proj_.near_ = near;

        float far = (float)proj_.far_;
        ImGui::SliderFloat(
            "Far", &far, 0.001f, 30000, nullptr, ImGuiSliderFlags_Logarithmic
        );
        proj_.far_ = far;

        ImGui::SliderFloat(
            "Exposure", &exposure_, 0, 10, nullptr, ImGuiSliderFlags_Logarithmic
        );

        ImGui::SliderFloat("Gamma", &gamma_, 0, 3);
        gamma_ = std::round(gamma_ * 10.f) / 10.f;
    }

}  // namespace mirinae::cpnt
