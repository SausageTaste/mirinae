#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_states_transp_static(
        RpCreateBundle& cbundle
    );

    std::unique_ptr<IRpBase> create_rp_states_transp_skinned(
        RpCreateBundle& cbundle
    );

}  // namespace mirinae::rp
