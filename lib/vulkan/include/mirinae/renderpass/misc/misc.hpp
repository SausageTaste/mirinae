#pragma once

#include "mirinae/lightweight/debug_ren.hpp"
#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_debug(RpCreateBundle& bundle);

}  // namespace mirinae::rp
