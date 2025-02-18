#pragma once

#include <sung/basic/time.hpp>

#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    class StandardCamera {

    public:
        void render_imgui(const sung::SimClock& clock);

        PerspectiveCamera<double> proj_;
        float exposure_ = 1;
        float gamma_ = 1;
    };

}  // namespace mirinae::cpnt
