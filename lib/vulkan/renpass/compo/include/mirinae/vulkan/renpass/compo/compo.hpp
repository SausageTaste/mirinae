#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_dlight(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rps_slight(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rps_vplight(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rps_envmap(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rps_sky(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rps_sky_atmos(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rps_atmos_surface(RpCreateBundle& cbundle);

}  // namespace mirinae::rp::compo
