#pragma once

#include <sung/basic/time.hpp>


namespace mirinae::imgui {

    struct Widget {
        virtual ~Widget() = default;
        virtual void do_frame(const sung::SimClock& clock) {}
        virtual void render() {}
    };

}  // namespace mirinae::imgui
