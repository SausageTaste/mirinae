#pragma once

#include <sung/basic/time.hpp>

#include "mirinae/cpnt/common.hpp"


namespace mirinae {

    struct IEnvmapRenUnit {
        virtual ~IEnvmapRenUnit() = default;
    };

}  // namespace mirinae


namespace mirinae::cpnt {

    class Envmap {

    public:
        void render_imgui();

        sung::MonotonicRealtimeTimer last_updated_;
    };

}  // namespace mirinae::cpnt
