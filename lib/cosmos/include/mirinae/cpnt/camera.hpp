#pragma once

#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    class StandardCamera {

    public:
        void render_imgui();

        PerspectiveCamera<double> proj_;
        float exposure_ = 1;
        float gamma_ = 1;
    };

}  // namespace mirinae::cpnt
