#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae {

    class RenderPassPackage : public IRenderPassRegistry {

    public:
        void add(
            const std::string& name, std::unique_ptr<IRenderPassBundle>&& rp
        ) override;

        void init_render_passes(
            uint32_t width,
            uint32_t height,
            FbufImageBundle& fbuf_bundle,
            DesclayoutManager& desclayouts,
            Swapchain& swapchain,
            VulkanDevice& device
        );

        void destroy();

        const IRenderPassBundle& get(const std::string& name) const override;

    private:
        std::map<std::string, std::unique_ptr<IRenderPassBundle>> data_;
    };

}  // namespace mirinae
