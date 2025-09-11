#pragma once

#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    class StandardCamera {

    public:
        void render_imgui();

        PerspectiveCamera<double> proj_;
        float exposure_ = 1;
        float gamma_ = 1;
        float bloom_radius_ = 0.005;
        float bloom_strength_ = 0.03;
    };

}  // namespace mirinae::cpnt
