#include "mirinae/vulkan/base/renderee/atmos.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/vulkan/base/render/mem_cinfo.hpp"
#include "mirinae/vulkan/base/render/vkdebug.hpp"


namespace {

    using float3 = glm::vec3;
    using float4 = glm::vec4;


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


    struct AtmosphereParameters {

    public:
        void set_default_values();
        void render_imgui();

        float3& ground_albedo();
        float& radius_bottom();
        float& radius_top();

        float radius_bottom() const;
        float radius_top() const;

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


    float3& AtmosphereParameters::ground_albedo() {
        return ::get_xyz(ground_albedo_);
    }

    float& AtmosphereParameters::radius_bottom() { return ground_albedo_.w; }

    float& AtmosphereParameters::radius_top() { return rayleigh_scattering_.w; }

    float AtmosphereParameters::radius_bottom() const {
        return ground_albedo_.w;
    }

    float AtmosphereParameters::radius_top() const {
        return rayleigh_scattering_.w;
    }

    float& AtmosphereParameters::rayleigh_density_exp_scale() {
        return mie_scattering_.w;
    }

    float3& AtmosphereParameters::rayleigh_scattering() {
        return ::get_xyz(rayleigh_scattering_);
    }

    float& AtmosphereParameters::mie_density_exp_scale() {
        return mie_absorption_.w;
    }

    float3& AtmosphereParameters::mie_scattering() {
        return ::get_xyz(mie_scattering_);
    }

    float3& AtmosphereParameters::mie_extinction() {
        return ::get_xyz(mie_extinction_);
    }

    float3& AtmosphereParameters::mie_absorption() {
        return ::get_xyz(mie_absorption_);
    }

    float& AtmosphereParameters::mie_phase_g() { return mie_extinction_.w; }

    float& AtmosphereParameters::absorption_density_0_layer_width() {
        return absorption_extinction_.w;
    }

    float& AtmosphereParameters::absorption_density_0_constant() {
        return absorption_density_params_.x;
    }

    float& AtmosphereParameters::absorption_density_0_linear() {
        return absorption_density_params_.y;
    }

    float& AtmosphereParameters::absorption_density_1_constant() {
        return absorption_density_params_.z;
    }

    float& AtmosphereParameters::absorption_density_1_linear() {
        return absorption_density_params_.w;
    }

    float3& AtmosphereParameters::absorption_extinction() {
        return ::get_xyz(absorption_extinction_);
    }


    AtmosphereParameters convert_atmos_params(const mirinae::AtmosParams& src) {
        AtmosphereParameters dst;

        dst.ground_albedo() = src.ground_albedo_.scaled_color();
        dst.radius_bottom() = src.radius_bottom_;
        dst.radius_top() = src.radius_top_;

        dst.rayleigh_density_exp_scale() = src.rayleigh_density_exp_scale_;
        dst.rayleigh_scattering() = src.rayleigh_scattering_.scaled_color();

        dst.mie_density_exp_scale() = src.mie_density_exp_scale_;
        dst.mie_scattering() = src.mie_scattering_.scaled_color();
        dst.mie_extinction() = src.mie_extinction_.scaled_color();
        dst.mie_absorption() = src.mie_absorption_.scaled_color();
        dst.mie_phase_g() = src.mie_phase_g_;

        dst.absorption_density_0_layer_width() =
            src.absorption_density_0_layer_width_;
        dst.absorption_density_0_constant() =
            src.absorption_density_0_constant_;
        dst.absorption_density_0_linear() = src.absorption_density_0_linear_;
        dst.absorption_density_1_constant() =
            src.absorption_density_1_constant_;
        dst.absorption_density_1_linear() = src.absorption_density_1_linear_;
        dst.absorption_extinction() = src.absorption_extinction_.scaled_color();

        return dst;
    }

}  // namespace


// RenUnitAtmosEpic
namespace mirinae {

    RenUnitAtmosEpic::RenUnitAtmosEpic(
        uint32_t max_flight_count, VulkanDevice& device
    )
        : device_(device) {
        mirinae::BufferCreateInfo cinfo;
        cinfo.preset_ubuf<AtmosphereParameters>().prefer_host();

        mirinae::DebugAnnoName anno;

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& buffer = ubuf_.emplace_back();
            buffer.init(cinfo, device_.mem_alloc());

            const auto name = fmt::format("atmosphere_epic_ubuf_f{}", i);
            anno.set_handle(buffer.get())
                .set_type(VK_OBJECT_TYPE_BUFFER)
                .set_name(name.c_str())
                .apply(device_.logi_device());

            auto& span = ubuf_span_.emplace_back();
            span.buf_ = buffer.get();
            span.offset_ = 0;
            span.size_ = buffer.size();
        }
    }

    RenUnitAtmosEpic::~RenUnitAtmosEpic() {}

    const BufferSpan& RenUnitAtmosEpic::ubuf_at(const FrameIndex f_idx) const {
        return ubuf_span_.at(f_idx.get());
    }

    void RenUnitAtmosEpic::update_ubuf(
        const FrameIndex f_index, const void* data, size_t size
    ) {
        auto& buf = ubuf_.at(f_index.get());
        buf.set_data(data, size);
    }

}  // namespace mirinae


// TaskAtmosEpic
namespace mirinae {

    void TaskAtmosEpic::init(
        uint32_t max_flight_count,
        entt::registry& reg,
        RpCtxtBase& rp_ctxt,
        VulkanDevice& device
    ) {
        reg_ = &reg;
        rp_ctxt_ = &rp_ctxt;
        device_ = &device;
        max_flight_count_ = max_flight_count;
    }

    void TaskAtmosEpic::prepare() {
        this->set_size(reg_->view<cpnt::AtmosphereEpic>().size());
    }

    void TaskAtmosEpic::ExecuteRange(
        enki::TaskSetPartition range, uint32_t tid
    ) {
        namespace cpnt = mirinae::cpnt;

        auto& reg = *reg_;
        auto view = reg.view<cpnt::AtmosphereEpic>();
        auto begin = view.begin() + range.start;
        auto end = view.begin() + range.end;

        for (auto it = begin; it != end; ++it) {
            const auto e = *it;
            auto& atmos = reg.get<cpnt::AtmosphereEpic>(e);
            const auto atmos_params = convert_atmos_params(atmos.params_);

            auto ren_unit = atmos.ren_unit<RenUnitAtmosEpic>();
            if (!ren_unit) {
                atmos.ren_unit_ = std::make_unique<RenUnitAtmosEpic>(
                    max_flight_count_, *device_
                );
                ren_unit = atmos.ren_unit<RenUnitAtmosEpic>();
            }

            ren_unit->update_ubuf(
                rp_ctxt_->f_index_, &atmos_params, sizeof(atmos_params)
            );
        }
    }

}  // namespace mirinae
