#pragma once

#include "mirinae/cosmos.hpp"
#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_envmap(RpCreateBundle& cbundle);

}  // namespace mirinae::rp
