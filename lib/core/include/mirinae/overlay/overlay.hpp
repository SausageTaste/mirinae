#pragma once

#include "mirinae/overlay/iwidget.hpp"


namespace mirinae {

    class OverlayManager : public IInputProcessor {

    public:
        OverlayManager(
            uint32_t win_width,
            uint32_t win_height,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        );
        ~OverlayManager();

        void record_render(
            size_t frame_index,
            VkCommandBuffer cmd_buf,
            VkPipelineLayout pipe_layout
        );
        void on_fbuf_resize(uint32_t width, uint32_t height);

        bool on_key_event(const mirinae::key::Event& e) override;
        bool on_text_event(uint32_t c) override;
        bool on_mouse_event(const mouse::Event& e) override;

        std::vector<std::unique_ptr<IWidget>>::iterator begin();
        std::vector<std::unique_ptr<IWidget>>::iterator end();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
