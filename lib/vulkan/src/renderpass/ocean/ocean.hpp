#pragma once

#include "renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_h0(
        RpCreateBundle& bundle
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_hkt(
        RpCreateBundle& bundle
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_butterfly(
        RpCreateBundle& bundle
    );

    URpStates create_rp_ocean_naive_ift(RpCreateBundle& bundle);

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_post_ift(
        RpCreateBundle& bundle
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_tess(
        RpCreateBundle& bundle
    );

}  // namespace mirinae::rp
