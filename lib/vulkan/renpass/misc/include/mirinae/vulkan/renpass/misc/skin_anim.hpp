#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_skin_anim(RpCreateBundle&);

}  // namespace mirinae::rp
