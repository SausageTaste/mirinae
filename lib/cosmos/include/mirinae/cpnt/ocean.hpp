#pragma once

#include <array>

#include <glm/vec2.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    constexpr uint32_t OCEAN_CASCADE_COUNT = 3;


    class Ocean {

    public:
        struct Cascade {
            Cascade();
            float amplitude() const;

            glm::vec2 texco_offset_;
            glm::vec2 texco_scale_;
            float amplitude_;
            float jacobian_scale_;
            float cutoff_high_;
            float cutoff_low_;
            float lod_scale_;
            float L_;
            bool active_;
        };

    public:
        Ocean();

        void do_frame(const sung::SimClock& clock);
        void render_imgui(const sung::SimClock& clock);

        static double max_wavelen(double L);

    public:
        TransformQuat<double> transform_;
        std::array<Cascade, OCEAN_CASCADE_COUNT> cascades_;
        glm::dvec2 wind_dir_;
        double repeat_time_;
        double time_;
        float depth_;
        float fetch_;
        float foam_bias_;
        float foam_scale_;
        float lod_scale_;
        float spread_blend_;
        float swell_;
        float tile_size_;
        float trub_time_factor_;
        float wind_speed_;
        int cascade_imgui_idx_;
        int tile_count_x_;
        int tile_count_y_;
        bool play_;
    };


}  // namespace mirinae::cpnt
