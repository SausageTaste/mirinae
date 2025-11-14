#include "mirinae/cpnt/atmos.hpp"

#include <imgui.h>
#include <entt/entity/registry.hpp>


namespace {

    bool render_color_intensity(mirinae::ColorIntensity& ci) {
        bool output = false;

        ImGui::PushID(reinterpret_cast<const void*>(&ci));
        output |= ImGui::ColorEdit3("Color", &ci.color()[0]);
        output |= ImGui::SliderFloat(
            "Intensity",
            &ci.intensity(),
            0.0,
            10.0,
            "%.6f",
            ImGuiSliderFlags_Logarithmic
        );
        ImGui::PopID();

        return output;
    }

}  // namespace


// AtmosParams
namespace mirinae {

    void AtmosParams::set_default_values() {
        absorption_extinction_.set_scaled_color(0.00065, 0.00188, 0.00008);

        mie_density_exp_scale_ = -0.83333;
        absorption_density_0_layer_width_ = 25;
        absorption_density_0_constant_ = -0.66667;
        absorption_density_0_linear_ = 0.06667;
        absorption_density_1_constant_ = 2.66667;
        absorption_density_1_linear_ = -0.06667;

        rayleigh_density_exp_scale_ = -0.125;

        mie_phase_g_ = 0.8;
        rayleigh_scattering_.set_scaled_color(0.0058, 0.01356, 0.0331);
        mie_scattering_.set_scaled_color(0.004);
        mie_absorption_.set_scaled_color(0.00044);
        mie_extinction_.set_scaled_color(0.00444);

        ground_albedo_.set_scaled_color(0);
        radius_bottom_ = 6360;
        radius_top_ = 6460;
    }

    void AtmosParams::render_imgui() {
        {
            float radius = radius_bottom_;
            float thickness = std::abs(radius_top_ - radius_bottom_);
            ImGui::DragFloat("Radius (km)", &radius, 10);
            ImGui::SliderFloat("Thickness (km)", &thickness, 0, 1000);
            radius_bottom_ = radius;
            radius_top_ = radius + std::abs(thickness);

            ImGui::Text("Ground albedo");
            ::render_color_intensity(ground_albedo_);
        }
        ImGui::Separator();
        {
            ImGui::Text("Rayleigh scattering");
            ::render_color_intensity(rayleigh_scattering_);
        }
        ImGui::Separator();
        {
            ImGui::Text("Mie scattering");
            ::render_color_intensity(mie_scattering_);
            ImGui::Text("Mie extinction");
            ::render_color_intensity(mie_extinction_);
            ImGui::Text("Mie absorption");
            ::render_color_intensity(mie_absorption_);
        }
        ImGui::Separator();
        {
            ImGui::DragFloat(
                "Rayleigh density exp scale",
                &rayleigh_density_exp_scale_,
                0.001f,
                -10.f,
                10.f
            );
            ImGui::DragFloat(
                "Mie density exp scale",
                &mie_density_exp_scale_,
                0.001f,
                -10.f,
                10.f
            );
            ImGui::SliderFloat("Mie phase G", &mie_phase_g_, 0.01f, 1.f);
        }
        ImGui::Separator();
        {
            ImGui::DragFloat(
                "Absorption 0 layer with",
                &absorption_density_0_layer_width_,
                0.01f
            );
            ImGui::DragFloat(
                "Absorption 0 const", &absorption_density_0_constant_, 0.01f
            );
            ImGui::DragFloat(
                "Absorption 0 linear", &absorption_density_0_linear_, 0.01f
            );
            ImGui::DragFloat(
                "Absorption 1 const", &absorption_density_1_constant_, 0.01f
            );
            ImGui::DragFloat(
                "Absorption 1 linear", &absorption_density_1_linear_, 0.01f
            );

            ImGui::Text("Absorption extinction");
            ::render_color_intensity(absorption_extinction_);
        }
        ImGui::Separator();

        if (ImGui::Button("Reset")) {
            this->set_default_values();
        }
    }

}  // namespace mirinae


// AtmosphereEpic
namespace mirinae::cpnt {

    AtmosphereEpic::AtmosphereEpic() { params_.set_default_values(); }

    void AtmosphereEpic::render_imgui() { params_.render_imgui(); }

    float AtmosphereEpic::radius_bottom() const {
        return params_.radius_bottom_;
    }

    float AtmosphereEpic::radius_top() const { return params_.radius_top_; }

}  // namespace mirinae::cpnt
