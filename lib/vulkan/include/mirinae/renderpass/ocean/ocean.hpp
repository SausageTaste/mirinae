#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp::ocean {

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_tilde_h(
        RpCreateBundle& bundle
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_tilde_hkt(
        RpCreateBundle& bundle
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_butterfly(
        RpCreateBundle& bundle
    );

    URpStates create_rp_states_ocean_naive_ift(RpCreateBundle& bundle);

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_finalize(
        RpCreateBundle& bundle
    );

    URpStates create_rp_states_ocean_tess(RpCreateBundle& bundle);

}  // namespace mirinae::rp::ocean
