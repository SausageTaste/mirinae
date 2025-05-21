#include "mirinae/cpnt/terrain.hpp"

#include <imgui.h>


namespace mirinae::cpnt {

    Terrain::Terrain() : tess_factor_(0.2) {}

    void Terrain::render_imgui() {
        ImGui::Text("Height map: %s", height_map_path_.u8string().c_str());
        ImGui::Text("Albedo map: %s", albedo_map_path_.u8string().c_str());
        ImGui::Text("Size: %.2f x %.2f", terrain_width_, terrain_height_);
        ImGui::DragScalar(
            "Height scale", ImGuiDataType_Double, &height_scale_, 0.1f
        );
        ImGui::SliderFloat(
            "Tess factor", &tess_factor_, 0, 10, 0, ImGuiSliderFlags_Logarithmic
        );
    }

}  // namespace mirinae::cpnt
