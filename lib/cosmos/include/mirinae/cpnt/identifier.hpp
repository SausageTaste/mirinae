#pragma once

#include <array>


namespace mirinae::cpnt {

    class Id {

    public:
        void render_imgui();
        void set_name(const char* name);

        std::array<char, 128> name_{};
    };

}  // namespace mirinae::cpnt
