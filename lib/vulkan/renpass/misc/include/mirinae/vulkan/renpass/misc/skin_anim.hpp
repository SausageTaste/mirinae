#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_skin_anim(
        RpCreateBundle& bundle
    );

}  // namespace mirinae::rp
