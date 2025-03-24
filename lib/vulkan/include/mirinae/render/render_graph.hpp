#pragma once

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "mirinae/render/vkdevice.hpp"


namespace mirinae::rg {

    enum class ImageType {
        color,
        depth,
        stencil,
        depth_stencil,
        storage,
        swapchain,
    };


    enum class ImageSizeType {
        absolute,
        relative_to_swapchain,
    };


    class RenderGraphImage {

    public:
        RenderGraphImage(const std::string_view name) : name_(name) {}

        RenderGraphImage(const RenderGraphImage&) = delete;
        RenderGraphImage& operator=(const RenderGraphImage&) = delete;

        const std::string& name() const;
        ImageType deduce_type() const;

        RenderGraphImage& set_format(VkFormat x);
        RenderGraphImage& set_type(ImageType x);
        RenderGraphImage& set_size_type(ImageSizeType x);
        RenderGraphImage& set_width(uint32_t x);
        RenderGraphImage& set_height(uint32_t x);
        RenderGraphImage& set_depth(uint32_t x);

        RenderGraphImage& set_size_rel_swhain(double ratio_w, double ratio_h);
        RenderGraphImage& set_size_rel_swhain(double ratio) {
            return this->set_size_rel_swhain(ratio, ratio);
        }

    private:
        std::string name_;
        VkFormat format_ = VK_FORMAT_UNDEFINED;
        ImageType type_ = ImageType::color;
        ImageSizeType size_type_ = ImageSizeType::absolute;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t depth_ = 0;
    };


    class RenderGraphPass {

    public:
        RenderGraphPass(const std::string_view name) : name_(name) {}

        RenderGraphPass(const RenderGraphPass&) = delete;
        RenderGraphPass& operator=(const RenderGraphPass&) = delete;

        const std::string& name() const { return name_; }

        RenderGraphPass& add_input_atta(RenderGraphImage& img) {
            input_atta_.push_back(&img);
            return *this;
        }

        RenderGraphPass& add_input_tex(RenderGraphImage& img) {
            input_tex_.push_back(&img);
            return *this;
        }

        RenderGraphPass& add_output_atta(RenderGraphImage& img) {
            output_atta_.push_back(&img);
            return *this;
        }

    private:
        std::string name_;
        std::vector<RenderGraphImage*> input_atta_;
        std::vector<RenderGraphImage*> input_tex_;
        std::vector<RenderGraphImage*> output_atta_;
    };


    class RenderGraphDef {

    public:
        RenderGraphDef();
        ~RenderGraphDef();

        RenderGraphImage& new_img(const std::string_view name);
        RenderGraphImage& get_img(const std::string_view name);

        RenderGraphPass& new_pass(const std::string_view name);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
