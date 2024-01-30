#include "mirinae/render/overlay.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"


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


namespace {

    class FontLibrary {

    public:
        FontLibrary(mirinae::TextureManager& tex_man, mirinae::VulkanDevice& device) {
            if (!device.filesys().read_file_to_vector("asset/font/SeoulNamsanM.ttf", file_data_))
                throw std::runtime_error("failed to load font file");

            stbtt_InitFont(&font_, reinterpret_cast<const unsigned char*>(file_data_.data()), 0);

            std::array<uint8_t, 512 * 512> temp_bitmap;
            stbtt_BakeFontBitmap(font_.data, 0, 32.0, temp_bitmap.data(), 512, 512, 32, 96, char_baked_.data());
            bitmap_.init(temp_bitmap.data(), 512, 512, 1);
            texture_ = tex_man.create_image("glyphs_ascii", bitmap_, false);
        }

        auto& ascii_texture() const { return *texture_; }

    private:
        stbtt_fontinfo font_;
        std::vector<uint8_t> file_data_;
        std::array<stbtt_bakedchar, 96> char_baked_; // ASCII 32..126 is 95 glyphs
        mirinae::TImage2D<unsigned char> bitmap_;
        std::unique_ptr<mirinae::ITexture> texture_;

    };


    class TextWidget : public mirinae::IWidget {

    public:
        TextWidget(VkSampler sampler, FontLibrary& fonts, mirinae::DesclayoutManager& desclayout, mirinae::TextureManager& tex_man, mirinae::VulkanDevice& device) {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/white.png")->image_view(),
                fonts.ascii_texture().image_view(),
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

                mirinae::U_OverlayPushConst push_const;
                push_const.pos_offset = overlay.ubuf_data_.offset();
                push_const.pos_scale = overlay.ubuf_data_.size();
                push_const.uv_offset = glm::vec2(0, 0);
                push_const.uv_scale = glm::vec2(2, 2);
                vkCmdPushConstants(
                    cmd_buf,
                    pipe_layout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(push_const),
                    &push_const
                );

                vkCmdDraw(cmd_buf, 6, 1, 0, 0);
            }
        }

        void on_parent_resize(double width, double height) override {
            for (auto& overlay : render_units_) {
                overlay.ubuf_data_.offset().x = (width - 10 - 512) / width * 2 - 1;
                overlay.ubuf_data_.offset().y = (height - 10 - 512) / height * 2 - 1;
                overlay.ubuf_data_.size().x = 512 / width;
                overlay.ubuf_data_.size().y = 512 / height;

                for (size_t i = 0; i < overlay.ubuf_count(); ++i)
                    overlay.udpate_ubuf(i);
            }
        }

        std::vector<mirinae::OverlayRenderUnit> render_units_;

    };

}


namespace mirinae {

    class OverlayManager::Impl {

    public:
        Impl(
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
            , font_lib_(tex_man, device)
            , wid_width_(win_width)
            , wid_height_(win_height)
        {
            SamplerBuilder sampler_builder;
            sampler_.reset(sampler_builder.build(device_));
        }

        mirinae::VulkanDevice& device_;
        mirinae::TextureManager& tex_man_;
        mirinae::DesclayoutManager& desclayout_;
        mirinae::Sampler sampler_;
        ::FontLibrary font_lib_;
        double wid_width_ = 0;
        double wid_height_ = 0;

        std::vector<std::unique_ptr<IWidget>> widgets_;

    };


    OverlayManager::OverlayManager(
        uint32_t win_width,
        uint32_t win_height,
        mirinae::DesclayoutManager& desclayout,
        mirinae::TextureManager& tex_man,
        mirinae::VulkanDevice& device
    )
        : pimpl_(std::make_unique<Impl>(win_width, win_height, desclayout, tex_man, device))
    {

    }

    OverlayManager::~OverlayManager() = default;

    void OverlayManager::on_fbuf_resize(uint32_t width, uint32_t height) {
        pimpl_->wid_width_ = width;
        pimpl_->wid_height_ = height;

        for (auto& widget : pimpl_->widgets_)
            widget->on_parent_resize(pimpl_->wid_width_, pimpl_->wid_height_);
    }

    void OverlayManager::add_widget_test() {
        auto widget = std::make_unique<::TextWidget>(pimpl_->sampler_.get(), pimpl_->font_lib_, pimpl_->desclayout_, pimpl_->tex_man_, pimpl_->device_);
        widget->on_parent_resize(pimpl_->wid_width_, pimpl_->wid_height_);
        pimpl_->widgets_.emplace_back(std::move(widget));
    }

    std::vector<std::unique_ptr<IWidget>>::iterator OverlayManager::begin() {
        return pimpl_->widgets_.begin();
    }

    std::vector<std::unique_ptr<IWidget>>::iterator OverlayManager::end() {
        return pimpl_->widgets_.end();
    }

}
