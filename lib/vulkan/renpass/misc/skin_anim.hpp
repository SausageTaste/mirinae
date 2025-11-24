#pragma once

#include "mirinae/lightweight/debug_ren.hpp"

#include "renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_skin_anim(
        RpCreateBundle& bundle
    );

}  // namespace mirinae::rp
