#include "mirinae/cpnt/ocean.hpp"

#include <imgui.h>


namespace {

    constexpr int INIT_L = 20;
    constexpr float AMP_BASE = 50000000;

}  // namespace


// Cascade
namespace mirinae::cpnt {

    Ocean::Cascade::Cascade()
        : texco_offset_{ 0, 0 }
        , texco_scale_{ 1, 0 }
        , amplitude_(AMP_BASE)
        , jacobian_scale_(1)
        , cutoff_high_(0)
        , cutoff_low_(0)
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
        , repeat_time_(100)
        , time_(0)
        , depth_(1000)
        , fetch_(50)
        , foam_bias_(2.3f)
        , foam_scale_(1.5f)
        , lod_scale_(100)
        , roughness_(0.01)
        , spread_blend_(0.7f)
        , swell_(0.7f)
        , tile_size_(20)
        , trub_time_factor_(0.5f)
        , wind_speed_(40)
        , cascade_imgui_idx_(0)
        , tile_count_x_(10)
        , tile_count_y_(10)
        , play_(true) {
        const auto max_wl = (float)this->max_wavelen(20);

        cascades_[0].L_ = 80;
        cascades_[0].lod_scale_ = 80;
        cascades_[0].cutoff_low_ = 0;
        cascades_[0].cutoff_high_ = 1;
        cascades_[0].jacobian_scale_ = 0.5;

        cascades_[1].L_ = 20;
        cascades_[1].lod_scale_ = 20;
        cascades_[1].cutoff_low_ = 1;
        cascades_[1].cutoff_high_ = 5;

        cascades_[2].L_ = 5;
        cascades_[2].lod_scale_ = 5;
        cascades_[2].cutoff_low_ = 5;
        cascades_[2].cutoff_high_ = 100;
    }

    void Ocean::do_frame(const sung::SimClock& clock) {
        if (play_)
            time_ += clock.dt();
    }

    void Ocean::render_imgui() {
        constexpr auto flog = ImGuiSliderFlags_Logarithmic;

        if (ImGui::Button("Play"))
            play_ = !play_;
        ImGui::DragScalar("Time", ImGuiDataType_Double, &time_, 0.1f);
        ImGui::DragScalar(
            "Repeat time", ImGuiDataType_Double, &repeat_time_, 0.1f
        );

        ImGui::DragScalar("Height", ImGuiDataType_Double, &height_, 0.1f);

        ImGui::SliderFloat("Roughness", &roughness_, 0, 1, 0, flog);
        ImGui::SliderFloat("Wind speed", &wind_speed_, 0.001f, 1000, 0, flog);
        ImGui::SliderFloat("Fetch", &fetch_, 0, 1000000, 0, flog);
        ImGui::SliderFloat("Depth", &depth_, 0.0000001f, 5000, "%.6f", flog);
        ImGui::SliderFloat("Swell", &swell_, 0, 1);
        ImGui::SliderFloat("Spread blend", &spread_blend_, 0, 1);

        float wind_dir = sung::to_degrees(std::atan2(wind_dir_.y, wind_dir_.x));
        ImGui::SliderFloat("Wind dir", &wind_dir, -179, 179);
        wind_dir_ = glm::vec2{ std::cos(sung::to_radians(wind_dir)),
                               std::sin(sung::to_radians(wind_dir)) };

        ImGui::SliderFloat(
            "Turb Time Factor", &trub_time_factor_, 0.01f, 2, 0, flog
        );
        ImGui::SliderFloat("Foam Bias", &foam_bias_, -10, 10);
        ImGui::SliderFloat("Foam Scale", &foam_scale_, 0, 10);
        ImGui::SliderFloat("LOD Scale", &lod_scale_, 0, 10000, 0, flog);
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
            ImGui::SliderFloat("Amplitude", &c.amplitude_, 0.01f, 10, 0, flog);
            ImGui::SliderFloat(
                "Jacobian scale", &c.jacobian_scale_, 0, 10, 0, flog
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
