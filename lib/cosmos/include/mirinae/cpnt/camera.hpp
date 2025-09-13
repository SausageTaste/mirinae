#pragma once

#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    class StandardCamera {

    public:
        StandardCamera();
        void render_imgui();

        PerspectiveCamera<double> proj_;
        float exposure_;
        float gamma_;
        float bloom_radius_;
        float bloom_strength_;
    };

}  // namespace mirinae::cpnt
