#include "mirinae/cpnt/identifier.hpp"

#include <imgui.h>


namespace mirinae::cpnt {

    void Id::render_imgui(const sung::SimClock& clock) {
        ImGui::InputText("Name", name_.data(), name_.size());
    }

    void Id::set_name(const char* name) {
        strncpy_s(name_.data(), name_.size(), name, _TRUNCATE);
    }

}  // namespace mirinae::cpnt
