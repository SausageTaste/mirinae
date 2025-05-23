#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/envmap/envmap.hpp"

#include <set>

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/envmap.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/envmap/cubemap.hpp"
#include "mirinae/renderpass/envmap/rp.hpp"


namespace {

    struct LocalRpReg : public mirinae::IEnvmapRpBundle {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) {
            base_ = mirinae::create_rp_base(desclayouts, device);
            diffuse_ = mirinae::create_rp_diffuse(desclayouts, device);
            specular_ = mirinae::create_rp_specular(desclayouts, device);
            sky_ = mirinae::create_rp_sky(desclayouts, device);
            brdf_lut_ = mirinae::create_rp_brdf_lut(desclayouts, device);
        }

        const mirinae::IRenPass& rp_base() const override { return *base_; }

        const mirinae::IRenPass& rp_diffuse() const override {
            return *diffuse_;
        }

        const mirinae::IRenPass& rp_specular() const override {
            return *specular_;
        }

        const mirinae::IRenPass& rp_sky() const override { return *sky_; }

        const mirinae::IRenPass& rp_brdf_lut() const override {
            return *brdf_lut_;
        }

    private:
        std::unique_ptr<mirinae::IRenPass> base_;
        std::unique_ptr<mirinae::IRenPass> diffuse_;
        std::unique_ptr<mirinae::IRenPass> specular_;
        std::unique_ptr<mirinae::IRenPass> sky_;
        std::unique_ptr<mirinae::IRenPass> brdf_lut_;
    };

}  // namespace


namespace {

    class RpMaster : public mirinae::IRpBase {

    public:
        RpMaster(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : cosmos_(cosmos), rp_res_(rp_res), device_(device) {
            auto& tex_man = *rp_res.tex_man_;
            rp_pkg_.init(rp_res.desclays_, device_);

            desc_pool_.init(
                5,
                rp_res.desclays_.get("envdiffuse:main").size_info() +
                    rp_res.desclays_.get("env_sky:main").size_info(),
                device_.logi_device()
            );

            // Sky texture
            {
                auto e = this->select_atmos_simple(cosmos.reg());
                auto& atmos = cosmos.reg().get<mirinae::cpnt::AtmosphereSimple>(
                    e
                );
                if (tex_man.request_blck(atmos.sky_tex_path_, false)) {
                    sky_tex_ = tex_man.get(atmos.sky_tex_path_);
                } else {
                    sky_tex_ = tex_man.missing_tex();
                }
            }

            desc_set_ = desc_pool_.alloc(
                rp_res.desclays_.get("env_sky:main").layout(),
                device_.logi_device()
            );

            mirinae::DescWriteInfoBuilder desc_info;
            desc_info.set_descset(desc_set_)
                .add_img_sampler(
                    sky_tex_->image_view(), device_.samplers().get_linear()
                )
                .apply_all(device_.logi_device());

            envmaps_ = std::make_unique<mirinae::EnvmapBundle>(
                rp_pkg_, device_
            );
            envmaps_->add(rp_pkg_, desc_pool_, rp_res.desclays_);
            rp_res.envmaps_ = envmaps_;
        }

        ~RpMaster() override {
            envmaps_->destroy();
            desc_pool_.destroy(device_.logi_device());
        }

        std::string_view name() const override { return "envmap"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            return nullptr;
        }

    private:
        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        ::LocalRpReg rp_pkg_;
        mirinae::DescPool desc_pool_;
        std::shared_ptr<mirinae::EnvmapBundle> envmaps_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        sung::MonotonicRealtimeTimer timer_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;  // For env sky

        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;
        mirinae::VulkanDevice& device_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_envmap(RpCreateBundle& cbundle) {
        return std::make_unique<RpMaster>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp
