#include "mirinae/cpnt/ocean.hpp"

#include <imgui.h>


namespace {

    constexpr int INIT_L = 20;

}  // namespace


// Cascade
namespace mirinae::cpnt {

    Ocean::Cascade::Cascade()
        : texco_offset_{ 0, 0 }
        , texco_scale_{ 1, 0 }
        , amplitude_(1)
        , jacobian_scale_(1)
        , cutoff_high_(10000)
        , cutoff_low_(0.0001)
        , lod_scale_(INIT_L)
        , L_(INIT_L)
        , active_(true) {}

    float Ocean::Cascade::amplitude() const { return active_ ? amplitude_ : 0; }

}  // namespace mirinae::cpnt


// Ocean
namespace mirinae::cpnt {

    Ocean::Ocean()
        : ocean_color_(0.1, 0.15, 0.25)
        , wind_dir_{ 1, 1 }
        , height_(0)
        , repeat_time_(1000)
        , time_(0)
        , depth_(500)
        , fetch_(1000000)
        , foam_bias_(2.7)
        , foam_scale_(1)
        , lod_scale_(100)
        , roughness_(0.01)
        , spread_blend_(0.5)
        , swell_(0.5)
        , tile_size_(20)
        , trub_time_factor_(0.5)
        , wind_speed_(1)
        , tess_factor_(1)
        , cascade_imgui_idx_(0)
        , tile_count_x_(10)
        , tile_count_y_(10)
        , play_(true) {}

    void Ocean::do_frame(const sung::SimClock& clock) {
        if (play_)
            time_ += clock.dt();
    }

    void Ocean::render_imgui() {
        constexpr auto L = ImGuiSliderFlags_Logarithmic;

        if (ImGui::Button("Play"))
            play_ = !play_;
        ImGui::DragScalar("Time", ImGuiDataType_Double, &time_, 0.1f);
        ImGui::DragScalar(
            "Repeat time", ImGuiDataType_Double, &repeat_time_, 0.1f
        );

        ImGui::DragScalar("Height", ImGuiDataType_Double, &height_, 0.1f);
        ImGui::SliderFloat("Tess factor", &tess_factor_, 0.f, 10.f, 0, L);

        ImGui::SliderFloat("Roughness", &roughness_, 0, 1, 0, L);
        ImGui::SliderFloat("Wind speed", &wind_speed_, 1e-6f, 100, "%.6f", L);
        ImGui::SliderFloat("Fetch", &fetch_, 0, 1000000, 0, L);
        ImGui::SliderFloat("Depth", &depth_, 1e-6f, 5000, "%.6f", L);
        ImGui::SliderFloat("Swell", &swell_, 0, 1);
        ImGui::SliderFloat("Spread blend", &spread_blend_, 0, 1);

        float wind_dir = sung::to_degrees(std::atan2(wind_dir_.y, wind_dir_.x));
        ImGui::SliderFloat("Wind dir", &wind_dir, -179, 179);
        wind_dir_ = glm::vec2{ std::cos(sung::to_radians(wind_dir)),
                               std::sin(sung::to_radians(wind_dir)) };

        ImGui::SliderFloat(
            "Turb Time Factor", &trub_time_factor_, 0.01f, 100, 0, L
        );
        ImGui::SliderFloat("Foam Bias", &foam_bias_, -10, 10);
        ImGui::SliderFloat("Foam Scale", &foam_scale_, 0, 10);
        ImGui::SliderFloat("LOD Scale", &lod_scale_, 0, 10000, 0, L);
        ImGui::ColorEdit3("Ocean color", &ocean_color_.x);

        if (ImGui::CollapsingHeader("Cascade###Header")) {
            ImGui::Indent(10);
            ImGui::SliderInt(
                "Cascade", &cascade_imgui_idx_, 0, OCEAN_CASCADE_COUNT - 1
            );

            ImGui::PushID(cascade_imgui_idx_);
            ImGui::Text("Cascade %d", cascade_imgui_idx_);

            auto& c = cascades_[cascade_imgui_idx_];

            ImGui::Checkbox("Active", &c.active_);
            ImGui::SliderFloat("Amplitude", &c.amplitude_, 0.01f, 10, 0, L);
            ImGui::SliderFloat(
                "Jacobian scale", &c.jacobian_scale_, 0, 10, 0, L
            );
            ImGui::DragFloat("LOD scale", &c.lod_scale_);
            ImGui::DragFloat("L", &c.L_);
            c.L_ = std::max(1.f, c.L_);

            const auto max_wl = (float)Ocean::max_wavelen(c.L_);
            ImGui::SliderFloat("Cut low", &c.cutoff_low_, 0, max_wl);
            ImGui::SliderFloat("Cut high", &c.cutoff_high_, 0, max_wl);

            ImGui::PopID();
            ImGui::Unindent(10);
        }
    }

    double Ocean::max_wavelen(double L) {
        return std::ceil(SUNG_PI * std::sqrt(2.0 * 256.0 * 256.0 / (L * L)));
    }

}  // namespace mirinae::cpnt
