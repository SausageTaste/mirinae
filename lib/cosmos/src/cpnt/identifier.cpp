#include "mirinae/cpnt/identifier.hpp"

#include <cstdio>

#include <imgui.h>


namespace mirinae::cpnt {

    void Id::render_imgui() {
        ImGui::InputText("Name", name_.data(), name_.size());
    }

    void Id::set_name(const char* name) {
        std::snprintf(name_.data(), name_.size(), "%s", name);
    }

}  // namespace mirinae::cpnt
