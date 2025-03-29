#include "mirinae/cpnt/terrain.hpp"

#include <imgui.h>


namespace mirinae::cpnt {

    Terrain::Terrain() : tess_factor_(0.2) {}

    void Terrain::render_imgui() {
        ImGui::Text("Height map: %s", height_map_path_.u8string().c_str());
        ImGui::Text("Albedo map: %s", albedo_map_path_.u8string().c_str());
        ImGui::SliderFloat(
            "Tess factor", &tess_factor_, 0, 10, 0, ImGuiSliderFlags_Logarithmic
        );
    }

}  // namespace mirinae::cpnt
