#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_bloom_upsample(RpCreateBundle& bundle);

}  // namespace mirinae::rp
