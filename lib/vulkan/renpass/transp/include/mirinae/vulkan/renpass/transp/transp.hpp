#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_states_transp_static(
        RpCreateBundle& cbundle
    );

}  // namespace mirinae::rp
