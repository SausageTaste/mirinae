#pragma once

#include "mirinae/cosmos.hpp"
#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_envmap(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rp_env_sky_atmos(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rp_env_mip_chain(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rp_env_diffuse(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rp_env_specular(RpCreateBundle& cbundle);

}  // namespace mirinae::rp
