#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_bloom_downsample(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_bloom_upsample(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_bloom_blend(RpCreateBundle&);

}  // namespace mirinae::rp
