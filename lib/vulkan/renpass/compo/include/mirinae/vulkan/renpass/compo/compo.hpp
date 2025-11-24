#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_dlight(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rps_slight(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rps_vplight(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rps_envmap(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rps_sky(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rps_sky_atmos(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rps_atmos_surface(RpCreateBundle&);

}  // namespace mirinae::rp::compo
