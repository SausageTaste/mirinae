#include "mirinae/cpnt/terrain.hpp"

#include <imgui.h>


namespace mirinae::cpnt {

    void Terrain::render_imgui(const sung::SimClock& clock) {
        ImGui::Text("Height map: %s", height_map_path_.u8string().c_str());
        ImGui::Text("Albedo map: %s", albedo_map_path_.u8string().c_str());
    }

}  // namespace mirinae::cpnt
