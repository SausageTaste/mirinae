#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae {

    class RenderPassPackage {

    public:
        void init(
            uint32_t width,
            uint32_t height,
            FbufImageBundle& fbuf_bundle,
            DesclayoutManager& desclayouts,
            Swapchain& swapchain,
            VulkanDevice& device
        );

        void destroy();

        const IRenderPassBundle& get(const std::string& name) const;

    private:
        RpMap data_;
    };

}  // namespace mirinae
