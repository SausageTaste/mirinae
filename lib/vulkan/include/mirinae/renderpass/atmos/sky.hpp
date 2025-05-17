#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_atmos_trans_lut(RpCreateBundle& bundle);
    std::unique_ptr<IRpBase> create_rp_atmos_multi_scat(RpCreateBundle& bundle);
    std::unique_ptr<IRpBase> create_rp_atmos_cam_vol(RpCreateBundle& bundle);
    std::unique_ptr<IRpBase> create_rp_sky_view_lut(RpCreateBundle& bundle);

}  // namespace mirinae::rp
