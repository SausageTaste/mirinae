#pragma once

#include "mirinae/render/renderee.hpp"


namespace mirinae {

    class IWidget {

    public:
        virtual ~IWidget() = default;
        virtual void record_render(size_t frame_index, VkCommandBuffer cmd_buf, VkPipelineLayout pipe_layout) = 0;
        virtual void on_parent_resize(double width, double height) = 0;

    };


    class OverlayManager {

    public:
        OverlayManager(
            uint32_t win_width,
            uint32_t win_height,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        );

        void add_widget(VkSampler sampler);

        void on_fbuf_resize(uint32_t width, uint32_t height);

        auto begin() {
            return widgets_.begin();
        }
        auto end() {
            return widgets_.end();
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::TextureManager& tex_man_;
        mirinae::DesclayoutManager& desclayout_;
        double wid_width_ = 0;
        double wid_height_ = 0;

        std::vector<std::unique_ptr<IWidget>> widgets_;

    };

}
