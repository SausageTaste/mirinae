#pragma once

#include <filesystem>

#include <sung/basic/time.hpp>


namespace mirinae {

    struct ITerrainRenUnit {
        virtual ~ITerrainRenUnit() = default;
    };

}  // namespace mirinae


namespace mirinae::cpnt {

    class Terrain {

    public:
        Terrain();

        void render_imgui();

        template <typename T>
        T* ren_unit() {
            return dynamic_cast<T*>(ren_unit_.get());
        }

        std::filesystem::path height_map_path_;
        std::filesystem::path albedo_map_path_;
        std::unique_ptr<ITerrainRenUnit> ren_unit_;
        float tess_factor_;
    };

}  // namespace mirinae::cpnt
