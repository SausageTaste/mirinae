#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_atmos_trans_lut(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_atmos_multi_scat(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_atmos_cam_vol(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_sky_view_lut(RpCreateBundle&);

}  // namespace mirinae::rp
