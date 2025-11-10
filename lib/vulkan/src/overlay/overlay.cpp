#include "mirinae/vulkan_pch.h"

#include "mirinae/overlay/overlay.hpp"

#include <SDL3/SDL_scancode.h>
#include <string_view>

#include "mirinae/lua/script.hpp"
#include "mirinae/overlay/text.hpp"
#include "mirinae/render/cmdbuf.hpp"


namespace {

    class ImageViewWidget : public mirinae::IRectWidget {

    public:
        ImageViewWidget(
            mirinae::DesclayoutManager& desclayout,
            mirinae::ITextureManager& tex_man,
            mirinae::VulkanDevice& device
        ) {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.block_for_tex(":asset/textures/black.ktx", true)
                    ->image_view(),
                tex_man.block_for_tex(":asset/textures/white.ktx", true)
                    ->image_view(),
                device.samplers().get_linear(),
                desclayout,
                tex_man
            );
        }

        ImageViewWidget(
            VkImageView color_img,
            VkImageView mask_img,
            mirinae::DesclayoutManager& desclayout,
            mirinae::ITextureManager& tex_man,
            mirinae::VulkanDevice& device
        ) {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                color_img,
                mask_img,
                device.samplers().get_linear(),
                desclayout,
                tex_man
            );
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            mirinae::DescSetBindInfo descset_info{ udata.pipe_layout_ };

            mirinae::PushConstInfo pc_info;
            pc_info.layout(udata.pipe_layout_)
                .add_stage_vert()
                .add_stage_frag();

            for (auto& overlay : render_units_) {
                descset_info.set(overlay.get_desc_set(udata.frame_index_))
                    .record(udata.cmd_buf_);
                pc_info.record(udata.cmd_buf_, overlay.push_const_);

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

            need_update_ = false;
        }

    private:
        std::vector<mirinae::OverlayRenderUnit> render_units_;
    };

}  // namespace


namespace {

    class LineEdit : public mirinae::IRectWidget {

    public:
        LineEdit(
            mirinae::TextRenderData& text_render_data,
            mirinae::DesclayoutManager& desclayout,
            mirinae::ITextureManager& tex_man,
            mirinae::VulkanDevice& device
        )
            : bg_img_(desclayout, tex_man, device)
            , text_box_(text_render_data) {
            text_box_.replace_osio(device.osio());
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            if (hidden_)
                return;

            text_box_.scroll_.y = -4;
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

            need_update_ = false;
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            return text_box_.on_key_event(e);
        }

        bool on_text_event(char32_t c) override {
            return text_box_.on_text_event(c);
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            return text_box_.on_mouse_event(e);
        }

        bool focused() const override { return text_box_.focused(); }

        void set_focus(bool focused) override { text_box_.set_focus(focused); }

        void add_text(const std::string_view str) { text_box_.add_text(str); }

        std::string flush_str() {
            const auto str = text_box_.make_str();
            text_box_.clear_text();
            return str;
        }

    private:
        ::ImageViewWidget bg_img_;
        mirinae::TextBox text_box_;
    };


    class DevConsole : public mirinae::IDevConsole {

    public:
        DevConsole(
            mirinae::TextRenderData& text_render_data,
            mirinae::DesclayoutManager& desclayout,
            mirinae::ITextureManager& tex_man,
            mirinae::ScriptEngine& script,
            mirinae::VulkanDevice& device
        )
            : script_(script) {
            {
                auto w = widgets_.emplace_back<LineEdit>(
                    text_render_data, desclayout, tex_man, device
                );
                w->pos_ = { 10, 415 };
                w->size_ = { 500, text_render_data.text_height() };
            }

            {
                auto w = widgets_.emplace_back<mirinae::TextBox>(
                    text_render_data
                );
                w->pos_ = { 10, 10 };
                w->size_ = { 500, 400 };
                w->replace_osio(device.osio());
                w->add_text("Hello, World!\n");
            }

            {
                auto w = widgets_.emplace_back<ImageViewWidget>(
                    desclayout, tex_man, device
                );
                w->pos_ = { 10, 10 };
                w->size_ = { 500, 400 };
            }
        }

        void tick(const mirinae::WidgetRenderUniData& ren_data) override {
            widgets_.tick(ren_data);
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            if (hidden_)
                return;
            widgets_.record_render(udata);
        }

        void update_content(const mirinae::WindowDimInfo& wd) override {
            widgets_.update_content(wd);
        }

        void request_update() override { widgets_.request_update(); };

        bool on_key_event(const mirinae::key::Event& e) override {
            using mirinae::key::ActionType;

            if (e.action_type == ActionType::up) {
                if (e.scancode_ == SDL_SCANCODE_GRAVE) {
                    if (this->hidden()) {
                        this->hide(false);
                        this->set_focus(true);
                    } else {
                        this->hide(true);
                        this->set_focus(false);
                    }

                    return true;
                }
            }

            if (hidden_)
                return false;

            if (e.scancode_ == SDL_SCANCODE_RETURN) {
                if (e.action_type == ActionType::down) {
                    auto line_edit = widgets_.find_by_type<LineEdit>();
                    const auto line = line_edit->flush_str();
                    if (!line.empty()) {
                        auto tb = widgets_.find_by_type<mirinae::TextBox>();
                        SPDLOG_INFO("Console command: '{}'", line);
                        tb->add_text(">> ");
                        tb->add_text(line);
                        tb->add_text("\n");
                        script_.exec(line.c_str());
                    }
                    return true;
                }
            }

            return widgets_.on_key_event(e);
        }

        bool on_text_event(char32_t c) override {
            if (hidden_)
                return false;
            if ('`' == c)
                return true;

            return widgets_.on_text_event(c);
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (hidden_)
                return false;

            return widgets_.on_mouse_event(e);
        }

        void hide(bool hidden) override { hidden_ = hidden; }

        bool hidden() const override { return hidden_; }

        void set_focus(bool focused) override { widgets_.set_focus(focused); }

        bool focused() const override { return widgets_.focused(); }

        void replace_output_buf(
            std::shared_ptr<mirinae::ITextData>& texts
        ) override {
            if (auto tb = widgets_.find_by_type<mirinae::TextBox>())
                tb->replace_text_buffer(texts);
        }

    private:
        mirinae::WidgetManager widgets_;
        mirinae::ScriptEngine& script_;
        bool hidden_ = false;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IDevConsole> create_dev_console(
        mirinae::TextRenderData& text_render_data,
        mirinae::DesclayoutManager& desclayout,
        mirinae::ITextureManager& tex_man,
        mirinae::ScriptEngine& script,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::DevConsole>(
            text_render_data, desclayout, tex_man, script, device
        );
    }


    class OverlayManager::Impl {

    public:
        Impl(
            uint32_t win_width,
            uint32_t win_height,
            mirinae::DesclayoutManager& desclayout,
            mirinae::ITextureManager& tex_man,
            mirinae::VulkanDevice& device
        )
            : device_(device)
            , tex_man_(tex_man)
            , desclayout_(desclayout)
            , font_lib_(device.filesys())
            , text_render_data_(device)
            , win_dim_(win_width, win_height, 1) {}

        TextRenderData& ascii_ren_data() {
            if (!text_render_data_.is_ready()) {
                text_render_data_.init_ascii(
                    font_lib_, desclayout_, tex_man_, device_
                );
            }

            return text_render_data_;
        }

        VulkanDevice& device_;
        ITextureManager& tex_man_;
        DesclayoutManager& desclayout_;

        WindowDimInfo win_dim_;
        FontLibrary font_lib_;

        key::EventAnalyzer key_states_;
        WidgetManager widgets_;

    private:
        TextRenderData text_render_data_;
    };


    OverlayManager::OverlayManager(
        uint32_t win_width,
        uint32_t win_height,
        mirinae::DesclayoutManager& desclayout,
        mirinae::ITextureManager& tex_man,
        mirinae::VulkanDevice& device
    )
        : pimpl_(
              std::make_unique<Impl>(
                  win_width, win_height, desclayout, tex_man, device
              )
          ) {}

    OverlayManager::~OverlayManager() = default;

    void OverlayManager::tick(WidgetRenderUniData& ren_data) {
        pimpl_->widgets_.record_render(ren_data);
    }

    void OverlayManager::record_render(WidgetRenderUniData& ren_data) {
        pimpl_->widgets_.record_render(ren_data);
    }

    void OverlayManager::on_fbuf_resize(uint32_t width, uint32_t height) {
        pimpl_->win_dim_ = WindowDimInfo{ static_cast<double>(width),
                                          static_cast<double>(height),
                                          1.0 };

        pimpl_->widgets_.request_update();
    }

    bool OverlayManager::on_key_event(const mirinae::key::Event& e) {
        pimpl_->key_states_.notify(e);
        auto new_e = e;
        new_e.states_ = &pimpl_->key_states_;
        return pimpl_->widgets_.on_key_event(new_e);
    }

    bool OverlayManager::on_text_event(char32_t c) {
        return pimpl_->widgets_.on_text_event(c);
    }

    bool OverlayManager::on_mouse_event(const mouse::Event& e) {
        return pimpl_->widgets_.on_mouse_event(e);
    }

    const WindowDimInfo& OverlayManager::win_dim() const {
        return pimpl_->win_dim_;
    }

    mirinae::TextRenderData& OverlayManager::text_render_data() {
        return pimpl_->ascii_ren_data();
    }

    WidgetManager& OverlayManager::widgets() { return pimpl_->widgets_; }

    WidgetManager const& OverlayManager::widgets() const {
        return pimpl_->widgets_;
    }

    void OverlayManager::create_image_view(VkImageView img_view, int x, int y) {
        auto w = std::make_unique<ImageViewWidget>(
            img_view,
            pimpl_->tex_man_.block_for_tex(":asset/textures/white.ktx", true)
                ->image_view(),
            pimpl_->desclayout_,
            pimpl_->tex_man_,
            pimpl_->device_
        );

        constexpr float SIZE = 256;
        constexpr float MARGIN = 10;

        w->pos_ = { MARGIN + (SIZE + MARGIN) * x,
                    MARGIN + (SIZE + MARGIN) * y };
        w->size_ = { SIZE, SIZE };
        w->update_content(pimpl_->win_dim_);
        this->widgets().add_widget(std::move(w));
    }

}  // namespace mirinae
