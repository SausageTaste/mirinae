#include "mirinae/overlay/text.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <sstream>


namespace {

    template <typename T>
    glm::tvec2<T> get_aabb_min(const sung::AABB2<T>& aabb) {
        return glm::tvec2<T>(aabb.x_min(), aabb.y_min());
    }

    template <typename T>
    glm::tvec2<T> get_aabb_dim(const sung::AABB2<T>& aabb) {
        return glm::tvec2<T>(aabb.width(), aabb.height());
    };

}  // namespace


// TextBlocks::Block
namespace mirinae {

    bool TextBlocks::Block::append(uint32_t value) {
        if ('\n' == value)
            ++new_line_count_;

        return data_.push_back(value);
    }

    bool TextBlocks::Block::pop_back() {
        if (data_.is_empty())
            return false;

        if ('\n' == data_.end()[-1])
            --new_line_count_;

        return data_.pop_back();
    }

    bool TextBlocks::Block::is_full() const { return data_.is_full(); }

    bool TextBlocks::Block::is_empty() const { return data_.is_empty(); }

    const uint32_t* TextBlocks::Block::begin() const { return data_.begin(); }

    const uint32_t* TextBlocks::Block::end() const { return data_.end(); }

}  // namespace mirinae


// TextBlocks
namespace mirinae {

    void TextBlocks::append(char c) {
        this->last_valid_block().append(static_cast<uint32_t>(c));
    }

    void TextBlocks::append(uint32_t c) { this->last_valid_block().append(c); }

    void TextBlocks::append(const std::string_view str) {
        for (auto c : str) {
            this->last_valid_block().append(static_cast<uint32_t>(c));
        }
    }

    void TextBlocks::pop_back() {
        const auto block_count = blocks_.size();
        for (size_t i = 0; i < block_count; ++i) {
            auto& block = blocks_[block_count - i - 1];

            if (block.pop_back())
                return;
            else
                blocks_.pop_back();
        }
    }

    void TextBlocks::clear() { blocks_.clear(); }

    std::string TextBlocks::make_str() const {
        std::ostringstream oss;

        for (const auto& block : blocks_) {
            for (auto c : block) {
                oss << static_cast<char>(c);
            }
        }

        return oss.str();
    }

    std::vector<TextBlocks::Block>::const_iterator TextBlocks::begin() const {
        return blocks_.begin();
    }

    std::vector<TextBlocks::Block>::const_iterator TextBlocks::end() const {
        return blocks_.end();
    }

    TextBlocks::Block& TextBlocks::last_valid_block() {
        if (blocks_.empty())
            blocks_.emplace_back();
        if (blocks_.back().is_full())
            blocks_.emplace_back();

        return blocks_.back();
    }

}  // namespace mirinae


// FontLibrary
namespace mirinae {

    FontLibrary::FontLibrary(mirinae::IFilesys& filesys) {
        const auto font_path = "asset/font/SeoulNamsanM.ttf";
        if (!filesys.read_file_to_vector(font_path, file_data_))
            throw std::runtime_error("failed to load font file");

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
        VkSampler sampler,
        FontLibrary& fonts,
        mirinae::DesclayoutManager& desclayout,
        mirinae::TextureManager& tex_man,
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
            tex_man.request("asset/textures/white.png", false)->image_view(),
            texture_->image_view(),
            sampler,
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
        texts_ = std::make_shared<TextBlocks>();
    }

    void TextBox::record_render(const mirinae::WidgetRenderUniData& udata) {
        auto desc_main = text_render_data_.get_desc_set(udata.frame_index_);
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

        const sung::AABB2<double> widget_box(
            pos_.x, pos_.x + size_.x, pos_.y, pos_.y + size_.y
        );
        auto x_offset = pos_.x;
        auto y_offset = pos_.y + text_render_data_.text_height();

        for (auto& block : *texts_) {
            for (auto c : block) {
                if ('\n' == c) {
                    x_offset = pos_.x;
                    y_offset += text_render_data_.text_height() * line_spacing_;
                    continue;
                }

                const auto& char_info = text_render_data_.get_char_info(c);
                const sung::AABB2<double> char_info_box(
                    char_info.x0, char_info.x1, char_info.y0, char_info.y1
                );
                const auto char_info_min = ::get_aabb_min(char_info_box);
                const auto char_info_dim = ::get_aabb_dim(char_info_box);
                const glm::dvec2 texture_dim(
                    text_render_data_.atlas_width(),
                    text_render_data_.atlas_height()
                );

                const sung::AABB2<double> glyph_box(
                    x_offset + char_info.xoff + scroll_.x,
                    x_offset + char_info.xoff + scroll_.x + char_info_dim.x,
                    y_offset + char_info.yoff + scroll_.y,
                    y_offset + char_info.yoff + scroll_.y + char_info_dim.y
                );

                mirinae::U_OverlayPushConst push_const;
                sung::AABB2<double> clipped_glyph_box;
                if (widget_box.make_intersection(
                        glyph_box, clipped_glyph_box
                    )) {
                    const auto clipped_glyph_area = clipped_glyph_box.area();
                    if (clipped_glyph_area <= 0.0) {
                        x_offset += char_info.xadvance;
                        continue;
                    } else if (clipped_glyph_area < glyph_box.area()) {
                        push_const.pos_offset = udata.pos_2_ndc(
                            clipped_glyph_box.x_min(), clipped_glyph_box.y_min()
                        );
                        push_const.pos_scale = udata.len_2_ndc(
                            clipped_glyph_box.width(),
                            clipped_glyph_box.height()
                        );
                        push_const.uv_scale =
                            char_info_dim * ::get_aabb_dim(clipped_glyph_box) /
                            (::get_aabb_dim(glyph_box) * texture_dim);
                        const auto texture_space_offset =
                            (::get_aabb_min(clipped_glyph_box) -
                             ::get_aabb_min(glyph_box)) *
                            char_info_dim / ::get_aabb_dim(glyph_box);
                        push_const.uv_offset = (char_info_min +
                                                texture_space_offset) /
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

                vkCmdPushConstants(
                    udata.cmd_buf_,
                    udata.pipe_layout_,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(push_const),
                    &push_const
                );

                vkCmdDraw(udata.cmd_buf_, 6, 1, 0, 0);
                x_offset += char_info.xadvance;
            }
        }
    }

    bool TextBox::on_mouse_event(const mirinae::mouse::Event& e) {
        using mirinae::mouse::ActionType;

        if (e.action_ == ActionType::down) {
            if (this->is_inside_cl(e.xpos_, e.ypos_)) {
                owning_mouse_ = true;
                last_mouse_pos_ = { e.xpos_, e.ypos_ };
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
    void TextBox::add_text(const uint32_t c) { texts_->append(c); }
    void TextBox::add_text(const std::string_view str) { texts_->append(str); }

    void TextBox::remove_one_char() { texts_->pop_back(); }
    void TextBox::clear_text() { texts_->clear(); }

}  // namespace mirinae
