#include "renderpasses.hpp"

#include "mirinae/vulkan/renpass/atmos/sky.hpp"
#include "mirinae/vulkan/renpass/bloom/bloom.hpp"
#include "mirinae/vulkan/renpass/compo/compo.hpp"
#include "mirinae/vulkan/renpass/envmap/envmap.hpp"
#include "mirinae/vulkan/renpass/gbuf/gbuf.hpp"
#include "mirinae/vulkan/renpass/misc/misc.hpp"
#include "mirinae/vulkan/renpass/misc/skin_anim.hpp"
#include "mirinae/vulkan/renpass/ocean/ocean.hpp"
#include "mirinae/vulkan/renpass/shadow/shadow.hpp"
#include "mirinae/vulkan/renpass/transp/transp.hpp"


namespace mirinae {

    HShadowMaps create_shadow_maps_bundle(VulkanDevice& device) {
        return rp::create_shadow_maps_bundle(device);
    }

    void create_gbuf_desc_layouts(
        DesclayoutManager& desclayouts, VulkanDevice& device
    ) {
        mirinae::rp::gbuf::create_desc_layouts(desclayouts, device);
    }

    std::vector<RpFactoryFunc> get_rp_factories() {
        return std::vector<RpFactoryFunc>{
            mirinae::rp::create_rp_skin_anim,
            mirinae::rp::create_rp_atmos_trans_lut,
            mirinae::rp::create_rp_atmos_multi_scat,
            mirinae::rp::create_rp_sky_view_lut,
            mirinae::rp::create_rp_atmos_cam_vol,
            mirinae::rp::create_rp_ocean_h0,
            mirinae::rp::create_rp_ocean_hkt,
            mirinae::rp::create_rp_ocean_butterfly,
            mirinae::rp::create_rp_ocean_post_ift,
            mirinae::rp::create_rp_envmap,
            mirinae::rp::create_rp_env_sky_atmos,
            mirinae::rp::create_rp_env_mip_chain,
            mirinae::rp::create_rp_env_diffuse,
            mirinae::rp::create_rp_env_specular,
            mirinae::rp::create_rp_shadow_static,
            mirinae::rp::create_rp_shadow_static_trs,
            mirinae::rp::create_rp_shadow_terrain,
            mirinae::rp::gbuf::create_rp_gbuf_static,
            mirinae::rp::gbuf::create_rp_gbuf_terrain,
            // mirinae::rp::compo::create_rps_dlight,
            mirinae::rp::compo::create_rps_atmos_surface,
            mirinae::rp::compo::create_rps_slight,
            mirinae::rp::compo::create_rps_envmap,
            mirinae::rp::compo::create_rps_sky_atmos,
            mirinae::rp::create_rp_ocean_tess,
            mirinae::rp::compo::create_rps_vplight,
            mirinae::rp::create_rp_states_transp_static,
            mirinae::rp::create_bloom_downsample,
            mirinae::rp::create_bloom_upsample,
            mirinae::rp::create_bloom_blend,
            mirinae::rp::create_rp_debug,
        };
    }

}  // namespace mirinae
