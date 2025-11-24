#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_ocean_h0(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_ocean_hkt(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_ocean_butterfly(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_ocean_post_ift(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_ocean_tess(RpCreateBundle&);

    URpStates create_rp_ocean_naive_ift(RpCreateBundle&);

}  // namespace mirinae::rp
