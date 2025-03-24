#pragma once

#include <deque>
#include <functional>
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


    enum class ImageCountType {
        single,
        per_frame,
    };


    class IRenderGraph;

    class IRenderPassImpl {

    public:
        virtual ~IRenderPassImpl() = default;
        virtual bool init(IRenderGraph& rg) = 0;
    };

    using URpImpl = std::unique_ptr<IRenderPassImpl>;


    class RenderGraphImage {

    public:
        RenderGraphImage(const std::string_view name) : name_(name) {}

        RenderGraphImage(const RenderGraphImage&) = delete;
        RenderGraphImage& operator=(const RenderGraphImage&) = delete;

        const std::string& name() const;
        VkFormat format() const { return format_; }
        ImageType img_type() const { return type_; }
        ImageCountType count_type() const { return count_type_; }
        ImageSizeType size_type() const { return size_type_; }
        uint32_t width() const { return width_; }
        uint32_t height() const { return height_; }
        uint32_t depth() const { return depth_; }

        RenderGraphImage& set_format(VkFormat x);
        RenderGraphImage& set_type(ImageType x);
        RenderGraphImage& set_size_type(ImageSizeType x);
        RenderGraphImage& set_count_type(ImageCountType x);
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
        ImageCountType count_type_ = ImageCountType::single;
        ImageSizeType size_type_ = ImageSizeType::absolute;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t depth_ = 0;
    };


    struct IRenderGraphPass {
        using impl_factory_t = std::function<URpImpl()>;

        virtual ~IRenderGraphPass() = default;

        virtual const std::string& name() const = 0;

        virtual IRenderGraphPass& set_impl_f(impl_factory_t factory) = 0;

        virtual IRenderGraphPass& add_in_tex(RenderGraphImage& img) = 0;
        virtual IRenderGraphPass& add_in_atta(RenderGraphImage& img) = 0;
        virtual IRenderGraphPass& add_out_atta(RenderGraphImage& img) = 0;

        IRenderGraphPass& add_inout_atta(RenderGraphImage& img) {
            return this->add_in_atta(img).add_out_atta(img);
        }
    };


    class IRenderGraph {

    public:
        struct IImage {
            virtual ~IImage() = default;
            virtual VkImageView img_view_at(uint32_t idx) const = 0;
        };

        struct IRenderPass {
            virtual ~IRenderPass() = default;
        };

    public:
        virtual ~IRenderGraph() = default;

        virtual IImage* get_img(std::string_view name) = 0;
        virtual IRenderPass* get_pass(std::string_view name) = 0;
    };


    class RenderGraphDef {

    public:
        RenderGraphDef();
        ~RenderGraphDef();

        RenderGraphImage& new_img(const std::string_view name);
        RenderGraphImage& get_img(const std::string_view name);

        IRenderGraphPass& new_pass(const std::string_view name);

        std::unique_ptr<IRenderGraph> build(
            Swapchain& swhain, VulkanDevice& device
        );

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae::rg
