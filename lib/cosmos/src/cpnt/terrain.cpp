#include "mirinae/cpnt/terrain.hpp"

#include <imgui.h>
#include <dal/auxiliary/path.hpp>


namespace mirinae::cpnt {

    Terrain::Terrain()
        : terrain_width_(0)
        , terrain_height_(0)
        , tile_count_x_(10)
        , tile_count_y_(10)
        , height_scale_(0)
        , tess_factor_(0.2) {}

    void Terrain::render_imgui() {
        ImGui::Text("Height map: %s", dal::tostr(height_map_path_).c_str());
        ImGui::Text("Albedo map: %s", dal::tostr(albedo_map_path_).c_str());
        ImGui::Text("Size: %.2f x %.2f", terrain_width_, terrain_height_);
        ImGui::Text("Tile count %d, %d", tile_count_x_, tile_count_y_);

        ImGui::DragScalar(
            "Height scale", ImGuiDataType_Double, &height_scale_, 0.1f
        );
        ImGui::SliderFloat(
            "Tess factor", &tess_factor_, 0, 10, 0, ImGuiSliderFlags_Logarithmic
        );
    }

}  // namespace mirinae::cpnt
