#include "overlay/text.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <SDL3/SDL_scancode.h>
#include <stb_truetype.h>

#include <sstream>

#include "mirinae/lightweight/include_spdlog.hpp"

#include "render/cmdbuf.hpp"


namespace {

    template <typename T>
    glm::tvec2<T> get_aabb_min(const sung::Aabb2D<T>& aabb) {
        return glm::tvec2<T>(aabb.x_min(), aabb.y_min());
    }

    template <typename T>
    glm::tvec2<T> get_aabb_dim(const sung::Aabb2D<T>& aabb) {
        return glm::tvec2<T>(aabb.width(), aabb.height());
    };


    bool is_ctrl_modifier_on(const mirinae::key::EventAnalyzer& states) {
        return states.is_pressed(SDL_SCANCODE_LCTRL) ||
               states.is_pressed(SDL_SCANCODE_RCTRL);
    }

    bool is_ctrl_modifier_on(const mirinae::key::Event& e) {
        if (!e.states_)
            return false;
        return ::is_ctrl_modifier_on(*e.states_);
    }

}  // namespace


// FontLibrary
namespace mirinae {

    FontLibrary::FontLibrary(dal::Filesystem& filesys) {
        const auto font_path = ":asset/font/SeoulNamsanM.ttf";
        if (!filesys.read_file(font_path, file_data_))
            MIRINAE_ABORT("failed to load font file");

        stbtt_InitFont(
            &font_, reinterpret_cast<const unsigned char*>(file_data_.data()), 0
        );
    }

}  // namespace mirinae


// TextRenderData
namespace mirinae {

    TextRenderData::TextRenderData(mirinae::VulkanDevice& device)
        : render_unit_(device) {}

    void TextRenderData::init_ascii(
        FontLibrary& fonts,
        mirinae::DesclayoutManager& desclayout,
        mirinae::ITextureManager& tex_man,
        mirinae::VulkanDevice& device
    ) {
        constexpr int w = 256;
        constexpr int h = 128;
        std::array<uint8_t, w * h> temp_bitmap;

        stbtt_BakeFontBitmap(
            fonts.data(),
            0,
            TEXT_HEIGHT,
            temp_bitmap.data(),
            w,
            h,
            START_CHAR,
            END_CHAR - START_CHAR,
            char_baked_.data()
        );
        bitmap_.init(temp_bitmap.data(), w, h, 1);
        texture_ = tex_man.create_image("glyphs_ascii", bitmap_, false);

        render_unit_.init(
            mirinae::MAX_FRAMES_IN_FLIGHT,
            tex_man.block_for_tex(":asset/textures/white.ktx", false)
                ->image_view(),
            texture_->image_view(),
            device.samplers().get_linear(),
            desclayout,
            tex_man
        );
    }

    bool TextRenderData::is_ready() const { return texture_ != nullptr; }

    VkDescriptorSet TextRenderData::get_desc_set(size_t frame_index) {
        return render_unit_.get_desc_set(frame_index);
    }

    stbtt_bakedchar& TextRenderData::get_char_info(size_t index) {
        const auto i = index - START_CHAR;
        if (i >= char_baked_.size())
            return char_baked_['?' - START_CHAR];
        else
            return char_baked_.at(i);
    }

    uint32_t TextRenderData::text_height() const { return TEXT_HEIGHT; }

    uint32_t TextRenderData::atlas_width() const { return texture_->width(); }

    uint32_t TextRenderData::atlas_height() const { return texture_->height(); }

}  // namespace mirinae


// TextBox
namespace mirinae {

    TextBox::TextBox(TextRenderData& text_render_data)
        : text_render_data_(text_render_data) {
        texts_ = mirinae::create_text_blocks();
    }

    void TextBox::record_render(const mirinae::WidgetRenderUniData& udata) {
        mirinae::DescSetBindInfo{}
            .layout(udata.pipe_layout_)
            .set(text_render_data_.get_desc_set(udata.frame_index_))
            .record(udata.cmd_buf_);

        const sung::Aabb2D<double> widget_box(
            pos_.x, pos_.x + size_.x, pos_.y, pos_.y + size_.y
        );
        auto x_offset = pos_.x;
        auto y_offset = pos_.y + text_render_data_.text_height();

        PushConstInfo pc_info{};
        pc_info.layout(udata.pipe_layout_).add_stage_vert().add_stage_frag();

        for (auto c : texts_->make_str32()) {
            if ('\n' == c) {
                x_offset = pos_.x;
                y_offset += text_render_data_.text_height() * line_spacing_;
                continue;
            }

            const auto& char_info = text_render_data_.get_char_info(c);
            const sung::Aabb2D<double> char_info_box(
                char_info.x0, char_info.x1, char_info.y0, char_info.y1
            );
            const auto char_info_min = ::get_aabb_min(char_info_box);
            const auto char_info_dim = ::get_aabb_dim(char_info_box);
            const glm::dvec2 texture_dim(
                text_render_data_.atlas_width(),
                text_render_data_.atlas_height()
            );

            const sung::Aabb2D<double> glyph_box(
                x_offset + char_info.xoff + scroll_.x,
                x_offset + char_info.xoff + scroll_.x + char_info_dim.x,
                y_offset + char_info.yoff + scroll_.y,
                y_offset + char_info.yoff + scroll_.y + char_info_dim.y
            );

            mirinae::U_OverlayPushConst push_const;
            sung::Aabb2D<double> clipped_glyph_box;
            if (widget_box.make_intersection(glyph_box, clipped_glyph_box)) {
                const auto clipped_glyph_area = clipped_glyph_box.area();
                if (clipped_glyph_area <= 0.0) {
                    x_offset += char_info.xadvance;
                    continue;
                } else if (clipped_glyph_area < glyph_box.area()) {
                    push_const.pos_offset = udata.pos_2_ndc(
                        clipped_glyph_box.x_min(), clipped_glyph_box.y_min()
                    );
                    push_const.pos_scale = udata.len_2_ndc(
                        clipped_glyph_box.width(), clipped_glyph_box.height()
                    );
                    push_const.uv_scale = char_info_dim *
                                          ::get_aabb_dim(clipped_glyph_box) /
                                          (::get_aabb_dim(glyph_box) *
                                           texture_dim);
                    const auto texture_space_offset =
                        (::get_aabb_min(clipped_glyph_box) -
                         ::get_aabb_min(glyph_box)) *
                        char_info_dim / ::get_aabb_dim(glyph_box);
                    push_const.uv_offset = (char_info_min + texture_space_offset
                                           ) /
                                           texture_dim;
                } else {
                    push_const.pos_offset = udata.pos_2_ndc(
                        glyph_box.x_min(), glyph_box.y_min()
                    );
                    push_const.pos_scale = udata.len_2_ndc(
                        glyph_box.width(), glyph_box.height()
                    );
                    push_const.uv_offset = char_info_min / texture_dim;
                    push_const.uv_scale = char_info_dim / texture_dim;
                }
            } else {
                x_offset += char_info.xadvance;
                continue;
            }

            push_const.color = { 1, 1, 1, 1 };

            pc_info.record(udata.cmd_buf_, push_const);

            vkCmdDraw(udata.cmd_buf_, 6, 1, 0, 0);
            x_offset += char_info.xadvance;
        }
    }

    bool TextBox::on_key_event(const mirinae::key::Event& e) {
        if (read_only_)
            return true;

        using mirinae::key::ActionType;

        if (e.action_type == ActionType::down) {
            if (::is_ctrl_modifier_on(e)) {
                if (e.scancode_ == SDL_SCANCODE_C) {
                    if (osio_)
                        osio_->set_clipboard(this->make_str());
                    else
                        SPDLOG_ERROR("OsIo was not given");
                } else if (e.scancode_ == SDL_SCANCODE_V) {
                    if (osio_) {
                        if (const auto str = osio_->get_clipboard())
                            this->add_text(str.value());
                        else
                            SPDLOG_ERROR("OsIo failed to get clipboard");
                    } else {
                        SPDLOG_ERROR("OsIo was not given");
                    }
                }
            } else {
                if (e.scancode_ == SDL_SCANCODE_BACKSPACE) {
                    this->remove_one_char();
                } else if (e.scancode_ == SDL_SCANCODE_RETURN) {
                    this->add_text('\n');
                }
            }
        }

        return true;
    }

    bool TextBox::on_text_event(char32_t c) {
        if (read_only_)
            return true;

        this->add_text(c);
        return true;
    }

    bool TextBox::on_mouse_event(const mirinae::mouse::Event& e) {
        using mirinae::mouse::ActionType;

        if (e.action_ == ActionType::down) {
            if (this->is_inside_cl(e.xpos_, e.ypos_)) {
                owning_mouse_ = true;
                last_mouse_pos_ = { e.xpos_, e.ypos_ };
            } else {
                owning_mouse_ = false;
            }
        } else if (e.action_ == ActionType::up) {
            owning_mouse_ = false;
        } else if (e.action_ == ActionType::move && owning_mouse_) {
            if (enable_scroll_) {
                scroll_.x += e.xpos_ - last_mouse_pos_.x;
                scroll_.y += e.ypos_ - last_mouse_pos_.y;
            }
            last_mouse_pos_ = { e.xpos_, e.ypos_ };
        }

        return owning_mouse_;
    }

    std::string TextBox::make_str() const { return texts_->make_str(); }

    void TextBox::add_text(const char c) { texts_->append(c); }
    void TextBox::add_text(const char32_t c) { texts_->append(c); }
    void TextBox::add_text(const std::string_view str) { texts_->append(str); }

    void TextBox::remove_one_char() { texts_->pop_back(); }
    void TextBox::clear_text() { texts_->clear(); }

    void TextBox::replace_text_buffer(std::shared_ptr<ITextData>& texts) {
        texts_ = texts;
    }
    void TextBox::replace_osio(IOsIoFunctions& osio) { osio_ = &osio; }

}  // namespace mirinae
