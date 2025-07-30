#include "mirinae/cpnt/atmos.hpp"


namespace {

    using float3 = mirinae::AtmosphereParameters::float3;
    using float4 = mirinae::AtmosphereParameters::float4;


    float3& get_xyz(float4& v) {
        static_assert(sizeof(float4) == sizeof(float) * 4);
        static_assert(sizeof(float4) * 3 == sizeof(float3) * 4);
        static_assert(offsetof(float4, x) == 0);
        static_assert(offsetof(float4, y) == sizeof(float));
        static_assert(offsetof(float4, z) == sizeof(float) * 2);
        static_assert(offsetof(float4, w) == sizeof(float) * 3);
        static_assert(offsetof(float3, x) == 0);
        static_assert(offsetof(float3, y) == sizeof(float));
        static_assert(offsetof(float3, z) == sizeof(float) * 2);

        return reinterpret_cast<float3&>(v);
    }

}  // namespace


// AtmosphereParameters
namespace mirinae {

#define CLS AtmosphereParameters

    void CLS::set_default_values() {
        this->absorption_extinction() = float3(0.00065, 0.00188, 0.00008);

        this->mie_density_exp_scale() = -0.83333;
        this->absorption_density_0_layer_width() = 25;
        this->absorption_density_0_constant() = -0.66667;
        this->absorption_density_0_linear() = 0.06667;
        this->absorption_density_1_constant() = 2.66667;
        this->absorption_density_1_linear() = -0.06667;

        this->rayleigh_density_exp_scale() = -0.125;

        this->mie_phase_g() = 0.8;
        this->rayleigh_scattering() = float3(0.0058, 0.01356, 0.0331);
        this->mie_scattering() = float3(0.004);
        this->mie_absorption() = float3(0.00044);
        this->mie_extinction() = float3(0.00444);

        this->ground_albedo() = float3(0);
        this->radius_bottom() = 6360;
        this->radius_top() = 6460;
    }

    float3& CLS::ground_albedo() { return ::get_xyz(ground_albedo_); }

    float& CLS::radius_bottom() { return ground_albedo_.w; }

    float& CLS::radius_top() { return rayleigh_scattering_.w; }

    float& CLS::rayleigh_density_exp_scale() { return mie_scattering_.w; }

    float3& CLS::rayleigh_scattering() {
        return ::get_xyz(rayleigh_scattering_);
    }

    float& CLS::mie_density_exp_scale() { return mie_absorption_.w; }

    float3& CLS::mie_scattering() { return ::get_xyz(mie_scattering_); }

    float3& CLS::mie_extinction() { return ::get_xyz(mie_extinction_); }

    float3& CLS::mie_absorption() { return ::get_xyz(mie_absorption_); }

    float& CLS::mie_phase_g() { return mie_extinction_.w; }

    float& CLS::absorption_density_0_layer_width() {
        return absorption_extinction_.w;
    }

    float& CLS::absorption_density_0_constant() {
        return absorption_density_params_.x;
    }

    float& CLS::absorption_density_0_linear() {
        return absorption_density_params_.y;
    }

    float& CLS::absorption_density_1_constant() {
        return absorption_density_params_.z;
    }

    float& CLS::absorption_density_1_linear() {
        return absorption_density_params_.w;
    }

    float3& CLS::absorption_extinction() {
        return ::get_xyz(absorption_extinction_);
    }

#undef CLS

}  // namespace mirinae


// AtmosphereEpic
namespace mirinae::cpnt {

    AtmosphereEpic::AtmosphereEpic() { atmos_params_.set_default_values(); }

    void AtmosphereEpic::render_imgui() {}

}  // namespace mirinae::cpnt
