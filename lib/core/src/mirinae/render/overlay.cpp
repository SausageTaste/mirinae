#include "mirinae/render/overlay.hpp"


namespace {

    class ImageView : public mirinae::IWidget {

    public:
        ImageView(VkSampler sampler, mirinae::DesclayoutManager& desclayout, mirinae::TextureManager& tex_man, mirinae::VulkanDevice& device) {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/lorem_ipsum.png")->image_view(),
                tex_man.request("asset/textures/white.png")->image_view(),
                sampler,
                desclayout,
                tex_man
            );
        }

        void record_render(size_t frame_index, VkCommandBuffer cmd_buf, VkPipelineLayout pipe_layout) override {
            for (auto& overlay : render_units_) {
                auto desc_main = overlay.get_desc_set(frame_index);
                vkCmdBindDescriptorSets(
                    cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipe_layout,
                    0,
                    1, &desc_main,
                    0, nullptr
                );

                vkCmdDraw(cmd_buf, 6, 1, 0, 0);
            }
        }

        void on_parent_resize(double width, double height) override {
            for (auto& overlay : render_units_) {
                overlay.ubuf_data_.offset().x = (width - 10 - 100) / width * 2 - 1;
                overlay.ubuf_data_.offset().y = (height - 10 - 100) / height * 2 - 1;
                overlay.ubuf_data_.size().x = 100 / width;
                overlay.ubuf_data_.size().y = 100 / height;

                for (size_t i = 0; i < overlay.ubuf_count(); ++i)
                    overlay.udpate_ubuf(i);
            }
        }

    private:
        std::vector<mirinae::OverlayRenderUnit> render_units_;

    };

}


namespace mirinae {

    OverlayManager::OverlayManager(
        uint32_t win_width,
        uint32_t win_height,
        mirinae::DesclayoutManager& desclayout,
        mirinae::TextureManager& tex_man,
        mirinae::VulkanDevice& device
    )
        : device_(device)
        , tex_man_(tex_man)
        , desclayout_(desclayout)
        , sampler_(device)
        , wid_width_(win_width)
        , wid_height_(win_height)
    {
        SamplerBuilder sampler_builder;
        sampler_.reset(sampler_builder.build(device_));
    }

    void OverlayManager::on_fbuf_resize(uint32_t width, uint32_t height) {
        wid_width_ = width;
        wid_height_ = height;

        for (auto& widget : widgets_)
            widget->on_parent_resize(wid_width_, wid_height_);
    }

    void OverlayManager::add_widget_test() {
        auto widget = std::make_unique<::ImageView>(sampler_.get(), desclayout_, tex_man_, device_);
        widget->on_parent_resize(wid_width_, wid_height_);
        widgets_.emplace_back(std::move(widget));
    }

}
