#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_bloom_downsample(RpCreateBundle& bundle);

}  // namespace mirinae::rp
