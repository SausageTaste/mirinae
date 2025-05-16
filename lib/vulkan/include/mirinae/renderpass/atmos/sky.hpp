#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_atmos_trans_lut(RpCreateBundle& bundle);

}  // namespace mirinae::rp
