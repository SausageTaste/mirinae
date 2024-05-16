#include "mirinae/overlay/overlay.hpp"

#include <string_view>

#include <spdlog/spdlog.h>

#include "mirinae/overlay/text.hpp"


namespace {

    class ImageView : public mirinae::IRectWidget {

    public:
        ImageView(
            VkSampler sampler,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        ) {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/black.png", true)->image_view(),
                tex_man.request("asset/textures/white.png", true)->image_view(),
                sampler,
                desclayout,
                tex_man
            );
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            for (auto& overlay : render_units_) {
                auto desc_main = overlay.get_desc_set(udata.frame_index_);
                vkCmdBindDescriptorSets(
                    udata.cmd_buf_,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    udata.pipe_layout_,
                    0,
                    1,
                    &desc_main,
                    0,
                    nullptr
                );

                vkCmdPushConstants(
                    udata.cmd_buf_,
                    udata.pipe_layout_,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(overlay.push_const_),
                    &overlay.push_const_
                );

                vkCmdDraw(udata.cmd_buf_, 6, 1, 0, 0);
            }
        }

        void update_content(const mirinae::WindowDimInfo& wd) override {
            for (auto& overlay : render_units_) {
                overlay.push_const_.pos_offset = wd.pos_2_ndc(pos_);
                overlay.push_const_.pos_scale = wd.len_2_ndc(size_);
                overlay.push_const_.uv_offset = { 0, 0 };
                overlay.push_const_.uv_scale = { 1, 1 };
            }
        }

    private:
        std::vector<mirinae::OverlayRenderUnit> render_units_;
    };

}  // namespace


namespace {

    class LineEdit : public mirinae::IRectWidget {

    public:
        LineEdit(
            VkSampler sampler,
            mirinae::TextRenderData& text_render_data,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        )
            : bg_img_(sampler, desclayout, tex_man, device)
            , text_box_(text_render_data) {
            text_box_.scroll_ = { 0, 0 };
            text_box_.enable_scroll_ = false;
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            if (hidden_)
                return;

            bg_img_.record_render(udata);
            text_box_.record_render(udata);
        }

        void update_content(const mirinae::WindowDimInfo& wd) override {
            bg_img_.pos_ = pos_;
            bg_img_.size_ = size_;
            bg_img_.update_content(wd);

            text_box_.pos_ = pos_;
            text_box_.size_ = size_;
            text_box_.update_content(wd);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (e.key == mirinae::key::KeyCode::backspace) {
                if (e.action_type == mirinae::key::ActionType::down) {
                    text_box_.remove_one_char();
                }
            }

            return true;
        }

        bool on_text_event(char32_t c) override {
            if (c == '`')
                return false;

            text_box_.add_text(c);
            return true;
        }

        void add_text(const std::string_view str) { text_box_.add_text(str); }

        std::string flush_str() {
            const auto str = text_box_.make_str();
            text_box_.clear_text();
            return str;
        }

    private:
        ::ImageView bg_img_;
        mirinae::TextBox text_box_;
    };


    class DevConsole : public mirinae::IWidget {

    public:
        DevConsole(
            VkSampler sampler,
            mirinae::TextRenderData& text_render_data,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        )
            : bg_img_text_box_(sampler, desclayout, tex_man, device)
            , text_box_(text_render_data)
            , line_edit_(
                  sampler, text_render_data, desclayout, tex_man, device
              ) {
            text_box_.pos_ = { 10, 10 };
            text_box_.size_ = { 500, 400 };
            text_box_.add_text("Hello, World!\n");

            bg_img_text_box_.pos_ = { 10, 10 };
            bg_img_text_box_.size_ = { 500, 400 };

            constexpr float LINE_EDIT_VER_MARGIN = 4;

            line_edit_.pos_ = { 10, 415 };
            line_edit_.size_ = { 500, text_render_data.text_height() };
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            line_edit_.record_render(udata);
            bg_img_text_box_.record_render(udata);
            text_box_.record_render(udata);
        }

        void update_content(const mirinae::WindowDimInfo& wd) override {
            bg_img_text_box_.update_content(wd);
            text_box_.update_content(wd);
            line_edit_.update_content(wd);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (e.key == mirinae::key::KeyCode::enter) {
                if (e.action_type == mirinae::key::ActionType::down) {
                    const auto line = line_edit_.flush_str();
                    if (!line.empty()) {
                        text_box_.add_text(line);
                        text_box_.add_text("\n");
                        spdlog::info("Console command: '{}'", line);
                    }
                }
            }

            if (line_edit_.on_key_event(e))
                return true;
            if (bg_img_text_box_.on_key_event(e))
                return true;
            if (text_box_.on_key_event(e))
                return true;

            return false;
        }

        bool on_text_event(char32_t c) override {
            if (line_edit_.on_text_event(c))
                return true;
            if (bg_img_text_box_.on_text_event(c))
                return true;
            if (text_box_.on_text_event(c))
                return true;

            return false;
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (line_edit_.on_mouse_event(e))
                return true;
            if (bg_img_text_box_.on_mouse_event(e))
                return true;
            if (text_box_.on_mouse_event(e))
                return true;

            return false;
        }

        void hide(bool hidden) override { hidden_ = hidden; }

        bool hidden() const override { return hidden_; }

    private:
        ImageView bg_img_text_box_;
        mirinae::TextBox text_box_;
        LineEdit line_edit_;
        bool hidden_ = false;
    };

}  // namespace


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
            , font_lib_(device.filesys())
            , text_render_data_(device)
            , win_dim_(win_width, win_height, 1) {
            SamplerBuilder sampler_builder;
            sampler_.reset(sampler_builder.build(device_));
        }

        DevConsole& get_or_create_dev_console() {
            if (auto w = widgets_.find_by_type<DevConsole>())
                return *w;

            auto w = widgets_.emplace_back<::DevConsole>(
                sampler_.get(),
                this->ascii_ren_data(),
                desclayout_,
                tex_man_,
                device_
            );

            w->update_content(win_dim_);
            w->hide(true);
            return *w;
        }

        TextRenderData& ascii_ren_data() {
            if (!text_render_data_.is_ready()) {
                text_render_data_.init_ascii(
                    sampler_.get(), font_lib_, desclayout_, tex_man_, device_
                );
            }

            return text_render_data_;
        }

        VulkanDevice& device_;
        TextureManager& tex_man_;
        DesclayoutManager& desclayout_;

        WindowDimInfo win_dim_;
        Sampler sampler_;
        FontLibrary font_lib_;

        WidgetManager widgets_;

    private:
        TextRenderData text_render_data_;
    };


    OverlayManager::OverlayManager(
        uint32_t win_width,
        uint32_t win_height,
        mirinae::DesclayoutManager& desclayout,
        mirinae::TextureManager& tex_man,
        mirinae::VulkanDevice& device
    )
        : pimpl_(std::make_unique<Impl>(
              win_width, win_height, desclayout, tex_man, device
          )) {}

    OverlayManager::~OverlayManager() = default;

    void OverlayManager::record_render(
        size_t frame_index,
        VkCommandBuffer cmd_buf,
        VkPipelineLayout pipe_layout
    ) {
        WidgetRenderUniData udata;
        udata.win_dim_ = pimpl_->win_dim_;
        udata.frame_index_ = frame_index;
        udata.cmd_buf_ = cmd_buf;
        udata.pipe_layout_ = pipe_layout;

        pimpl_->widgets_.record_render(udata);
    }

    void OverlayManager::on_fbuf_resize(uint32_t width, uint32_t height) {
        pimpl_->win_dim_ = WindowDimInfo{ static_cast<double>(width),
                                          static_cast<double>(height),
                                          1.0 };

        pimpl_->widgets_.request_update();
    }

    bool OverlayManager::on_key_event(const mirinae::key::Event& e) {
        if (e.action_type == key::ActionType::up) {
            if (e.key == key::KeyCode::backquote) {
                auto& w = pimpl_->get_or_create_dev_console();
                w.hide(!w.hidden());
                return true;
            }
        }

        return pimpl_->widgets_.on_key_event(e);
    }

    bool OverlayManager::on_text_event(char32_t c) {
        return pimpl_->widgets_.on_text_event(c);
    }

    bool OverlayManager::on_mouse_event(const mouse::Event& e) {
        return pimpl_->widgets_.on_mouse_event(e);
    }

}  // namespace mirinae
