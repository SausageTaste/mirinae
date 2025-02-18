#include "mirinae/cpnt/envmap.hpp"

#include <imgui.h>


namespace mirinae::cpnt {

    void Envmap::render_imgui(const sung::SimClock& clock) {
        ImGui::Text("Last updated: %f", last_updated_.elapsed());

        if (ImGui::Button("Update Now"))
            last_updated_.set_min();
    }

}  // namespace mirinae::cpnt
