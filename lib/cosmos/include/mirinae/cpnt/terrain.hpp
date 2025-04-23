#pragma once

#include <filesystem>

#include <daltools/img/img.hpp>
#include <sung/basic/time.hpp>


namespace mirinae {

    struct ITerrainRenUnit {
        virtual ~ITerrainRenUnit() = default;
        virtual const dal::IImage* height_map() const = 0;
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

        template <typename T>
        const T* ren_unit() const {
            return dynamic_cast<const T*>(ren_unit_.get());
        }

        std::filesystem::path height_map_path_;
        std::filesystem::path albedo_map_path_;
        std::unique_ptr<ITerrainRenUnit> ren_unit_;
        float tess_factor_;
    };

}  // namespace mirinae::cpnt
