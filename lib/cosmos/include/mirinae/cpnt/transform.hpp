#pragma once

#include <sung/basic/time.hpp>

#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    class Transform : public TransformQuat<double> {

    public:
        void render_imgui();
    };

}  // namespace mirinae::cpnt
