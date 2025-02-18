#pragma once

#include <sung/basic/time.hpp>

#include "mirinae/cpnt/common.hpp"


namespace mirinae {

    struct IEnvmapRenUnit {
        virtual ~IEnvmapRenUnit() = default;
    };

}  // namespace mirinae


namespace mirinae::cpnt {

    class Envmap : public RenUnitHolder<IEnvmapRenUnit> {

    public:
        void render_imgui(const sung::SimClock& clock);

        sung::MonotonicRealtimeTimer last_updated_;
    };

}  // namespace mirinae::cpnt
