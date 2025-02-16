#pragma once

#include <array>

#include <sung/basic/time.hpp>


namespace mirinae::cpnt {

    class Id {

    public:
        void render_imgui(const sung::SimClock& clock);

        void set_name(const char* name);

        std::array<char, 128> name_{};
    };

}  // namespace mirinae::cpnt
