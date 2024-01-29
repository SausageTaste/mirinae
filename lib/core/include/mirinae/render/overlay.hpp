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
        ~OverlayManager();

        void add_widget_test();

        void on_fbuf_resize(uint32_t width, uint32_t height);

        std::vector<std::unique_ptr<IWidget>>::iterator begin();
        std::vector<std::unique_ptr<IWidget>>::iterator end();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;

    };

}
