#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "mirinae/cpnt/common.hpp"


namespace mirinae {

    struct AtmosphereParameters {

    public:
        using float3 = glm::vec3;
        using float4 = glm::vec4;

    public:
        void set_default_values();

        float3& ground_albedo();
        float& radius_bottom();
        float& radius_top();

        float& rayleigh_density_exp_scale();
        float3& rayleigh_scattering();

        float& mie_density_exp_scale();
        float3& mie_scattering();
        float3& mie_extinction();
        float3& mie_absorption();
        float& mie_phase_g();

        float& absorption_density_0_layer_width();
        float& absorption_density_0_constant();
        float& absorption_density_0_linear();
        float& absorption_density_1_constant();
        float& absorption_density_1_linear();
        float3& absorption_extinction();

    private:
        // xyz: as-is, w: Radius bottom
        float4 ground_albedo_;
        // xyz: as-is, w: Radius top
        float4 rayleigh_scattering_;
        // xyz: as-is, w: Rayleigh density exp scale
        float4 mie_scattering_;
        // xyz: as-is, w: Mie phase g
        float4 mie_extinction_;
        // xyz: as-is, w: Mie density exp scale
        float4 mie_absorption_;
        // xyz: as-is, w: Absorption density 0 layer width
        float4 absorption_extinction_;

        // absorption_density_0_constant_term;
        // absorption_density_0_linear_term;
        // absorption_density_1_constant_term;
        // absorption_density_1_linear_term;
        float4 absorption_density_params_;
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

    public:
        AtmosphereParameters atmos_params_;
    };

}  // namespace mirinae::cpnt
