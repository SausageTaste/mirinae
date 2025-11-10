#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "mirinae/cpnt/common.hpp"
#include "mirinae/math/color.hpp"


namespace mirinae {

    class AtmosParams {

    public:
        void render_imgui();
        void set_default_values();

    public:
        ColorIntensity ground_albedo_;
        float radius_bottom_;
        float radius_top_;

        float rayleigh_density_exp_scale_;
        ColorIntensity rayleigh_scattering_;

        float mie_density_exp_scale_;
        ColorIntensity mie_scattering_;
        ColorIntensity mie_extinction_;
        ColorIntensity mie_absorption_;
        float mie_phase_g_;

        float absorption_density_0_layer_width_;
        float absorption_density_0_constant_;
        float absorption_density_0_linear_;
        float absorption_density_1_constant_;
        float absorption_density_1_linear_;
        ColorIntensity absorption_extinction_;
    };


    struct IAtmosEpicRenUnit {
        virtual ~IAtmosEpicRenUnit() = default;
    };

}  // namespace mirinae


namespace mirinae::cpnt {

    class AtmosphereEpic : public RenUnitHolder<IAtmosEpicRenUnit> {

    public:
        AtmosphereEpic();
        void render_imgui();

        static int lua_module(lua_State* L);

        float radius_bottom() const;
        float radius_top() const;

    public:
        AtmosParams params_;
    };

}  // namespace mirinae::cpnt
