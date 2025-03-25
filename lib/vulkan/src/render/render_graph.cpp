#include "mirinae/render/render_graph.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"


#define MIRINAE_GET_IMPL()                                        \
    if (!pimpl_)                                                  \
        MIRINAE_ABORT("RenderGraphDef::Impl is not initialized"); \
    auto& pimpl = *pimpl_;


// Aux for RenderGraphImage
namespace {

    constexpr double REL_SIZE_VALUE_FACTOR = 1000;

    mirinae::rg::ImageType classify_img_format(VkFormat format) {
        switch (format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
                return mirinae::rg::ImageType::depth;
            case VK_FORMAT_S8_UINT:
                return mirinae::rg::ImageType::stencil;
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return mirinae::rg::ImageType::depth_stencil;
            default:
                return mirinae::rg::ImageType::color;
        }
    }


    template <typename T>
    class NamedItemRegistry {

    public:
        T* find(const std::string_view name) {
            for (auto& x : data_) {
                if (x.name() == name) {
                    return &x;
                }
            }
            return nullptr;
        }

        template <typename... Args>
        bool append(Args&&... args) {
            data_.emplace_back(std::forward<Args>(args)...);
            return true;
        }

        auto begin() { return data_.begin(); }
        auto end() { return data_.end(); }
        auto begin() const { return data_.begin(); }
        auto end() const { return data_.end(); }

    private:
        std::deque<T> data_;
    };

}  // namespace


// RenderGraphImage
namespace mirinae::rg {

    const std::string& RenderGraphImage::name() const { return name_; }

    RenderGraphImage& RenderGraphImage::set_format(VkFormat x) {
        format_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_type(ImageType x) {
        type_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_count_type(ImageCountType x) {
        count_type_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_size_type(ImageSizeType x) {
        size_type_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_width(uint32_t x) {
        width_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_height(uint32_t x) {
        height_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_depth(uint32_t x) {
        depth_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_size_rel_swhain(
        double ratio_w, double ratio_h
    ) {
        size_type_ = ImageSizeType::relative_to_swapchain;
        width_ = ratio_w * REL_SIZE_VALUE_FACTOR;
        height_ = ratio_h * REL_SIZE_VALUE_FACTOR;
        return *this;
    }

}  // namespace mirinae::rg


// RenderGraphPass
namespace {

    class RenderGraphPass : public mirinae::rg::IRenderGraphPass {

    public:
        using ImageDef = mirinae::rg::RenderGraphImage;

    public:
        RenderGraphPass(const std::string_view name) : name_(name) {}

        const std::string& name() const override { return name_; }

        IRenderGraphPass& set_impl_f(impl_factory_t factory) override {
            impl_factory_ = std::move(factory);
            return *this;
        }

        IRenderGraphPass& add_in_tex(ImageDef& img) override {
            input_tex_.push_back(&img);
            return *this;
        }

        IRenderGraphPass& add_in_atta(ImageDef& img) override {
            input_atta_.push_back(&img);
            return *this;
        }

        IRenderGraphPass& add_out_atta(ImageDef& img) override {
            output_atta_.push_back(&img);
            return *this;
        }

        bool need_read(const ImageDef& img) const {
            for (auto& x : input_tex_) {
                if (x == &img) {
                    return true;
                }
            }
            for (auto& x : input_atta_) {
                if (x == &img) {
                    return true;
                }
            }
            return false;
        }

        bool need_write(const ImageDef& img) const {
            for (auto& x : output_atta_) {
                if (x == &img) {
                    return true;
                }
            }
            return false;
        }

        impl_factory_t impl_factory_;
        std::vector<ImageDef*> input_tex_;
        std::vector<ImageDef*> input_atta_;
        std::vector<ImageDef*> output_atta_;

    private:
        std::string name_;
    };

}  // namespace


// RenderGraph
namespace {

    class RgImage : public mirinae::rg::IRenderGraph::IImage {

    public:
        RgImage(
            const mirinae::rg::RenderGraphImage& src,
            const uint32_t max_frames_in_flight,
            const uint32_t swhain_width,
            const uint32_t swhain_height,
            mirinae::VulkanDevice& device
        ) {
            using ImageCountType = mirinae::rg::ImageCountType;
            using ImageSizeType = mirinae::rg::ImageSizeType;

            name_ = src.name();

            auto img_count = 1;
            if (src.count_type() == ImageCountType::per_frame)
                img_count = max_frames_in_flight;

            mirinae::ImageCreateInfo img_cinfo;
            if (src.size_type() == ImageSizeType::relative_to_swapchain) {
                const auto w = swhain_width * src.width() /
                               REL_SIZE_VALUE_FACTOR;
                const auto h = swhain_height * src.height() /
                               REL_SIZE_VALUE_FACTOR;
                img_cinfo.set_dimensions(w, h);
            } else {
                img_cinfo.set_dimensions(src.width(), src.height());
            }

            img_cinfo.set_format(src.format())
                .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                .add_usage(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

            if (src.img_type() == mirinae::rg::ImageType::depth) {
                img_cinfo.add_usage(
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                );
            } else {
                img_cinfo.add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
            }

            mirinae::ImageViewBuilder view_cinfo;
            view_cinfo.format(src.format());

            if (src.img_type() == mirinae::rg::ImageType::depth) {
                view_cinfo.aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT);
            } else {
                view_cinfo.aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);
            }

            for (auto i = 0; i < img_count; i++) {
                auto& img = img_.emplace_back();
                img.init(img_cinfo.get(), device.mem_alloc());

                view_cinfo.image(img.image());
                auto& view = img_view_.emplace_back();
                view.reset(view_cinfo, device);
            }
        }

        RgImage(const RgImage&) = delete;
        RgImage& operator=(const RgImage&) = delete;

        RgImage(RgImage&& src) noexcept {
            name_ = std::move(src.name_);
            img_ = std::move(src.img_);
            img_view_ = std::move(src.img_view_);
        }

        RgImage& operator=(RgImage&& src) noexcept {
            name_ = std::move(src.name_);
            img_ = std::move(src.img_);
            img_view_ = std::move(src.img_view_);
            return *this;
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& x : img_view_) x.destroy(device);
            for (auto& x : img_) x.destroy(device.mem_alloc());
        }

        const std::string& name() const { return name_; }

        const VkExtent2D extent2d() const { return img_.at(0).extent2d(); }

        VkImageView img_view_at(uint32_t idx) const override {
            return img_view_.at(idx).get();
        }

    private:
        std::string name_;
        std::vector<mirinae::Image> img_;
        std::vector<mirinae::ImageView> img_view_;
    };


    class RgPass : public mirinae::rg::IRenderGraph::IRenderPass {

    public:
        RgPass(
            const ::RenderGraphPass& src,
            const uint32_t max_frames_in_flight,
            ::NamedItemRegistry<RgImage>& images,
            mirinae::VulkanDevice& device
        )
            : def_(src) {
            using ImageCountType = mirinae::rg::ImageCountType;

            render_pass_.reset(this->create_rp(src, device), device);
            frame_data_.resize(max_frames_in_flight);
            fbuf_extent_ = this->find_extent2d(src, images);

            for (uint32_t i = 0; i < max_frames_in_flight; ++i) {
                auto& fd = frame_data_.at(i);
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_.get()).set_dim(fbuf_extent_);

                for (auto& img : src.output_atta_) {
                    uint32_t img_idx = 0;
                    if (img->count_type() == ImageCountType::per_frame)
                        img_idx = i;

                    auto src_img = images.find(img->name());
                    MIRINAE_ASSERT(nullptr != src_img);

                    fbuf_cinfo.add_attach(src_img->img_view_at(img_idx));
                }

                fd.fbuf_.init(fbuf_cinfo.get(), device.logi_device());
            }

            for (auto& img : src.output_atta_) {
                clear_values_.emplace_back();
                if (img->img_type() == mirinae::rg::ImageType::depth) {
                    clear_values_.back().depthStencil = { 0, 0 };
                } else {
                    clear_values_.back().color = { 0, 0, 0, 1 };
                }
            }

            if (src.impl_factory_)
                rpimpl_ = src.impl_factory_();
        }

        void destroy(mirinae::VulkanDevice& device) {
            if (rpimpl_)
                rpimpl_->destroy(device);
            rpimpl_.reset();

            for (auto& fd : frame_data_) {
                fd.fbuf_.destroy(device.logi_device());
            }

            render_pass_.destroy(device);
        }

        const std::string& name() const override { return def_.name(); }

        VkRenderPass rp() const override { return render_pass_.get(); }

        VkFramebuffer fbuf_at(mirinae::FrameIndex idx) const override {
            return frame_data_.at(idx.get()).fbuf_.get();
        }

        VkExtent2D fbuf_extent() const override { return fbuf_extent_; }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        static VkRenderPass create_rp(
            const ::RenderGraphPass& src, mirinae::VulkanDevice& device
        ) {
            mirinae::RenderPassBuilder builder;

            for (auto& img : src.output_atta_) {
                const auto idx = builder.attach_desc().size();

                auto img_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                if (img->img_type() == mirinae::rg::ImageType::depth)
                    img_layout =
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                auto& atta = builder.attach_desc().add(img->format());
                atta.ini_layout(img_layout).fin_layout(img_layout);

                if (src.need_read(*img))
                    atta.load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
                else
                    atta.load_op(VK_ATTACHMENT_LOAD_OP_CLEAR);

                if (src.need_write(*img))
                    atta.stor_op(VK_ATTACHMENT_STORE_OP_STORE);
                else
                    atta.stor_op(VK_ATTACHMENT_STORE_OP_DONT_CARE);

                if (img->img_type() == mirinae::rg::ImageType::depth)
                    builder.depth_attach_ref().set(idx);
                else
                    builder.color_attach_ref().add_color_attach(idx);
            }

            builder.subpass_dep().add().preset_single();

            return builder.build(device.logi_device());
        }

        static VkExtent2D find_extent2d(
            const ::RenderGraphPass& src, ::NamedItemRegistry<RgImage>& images
        ) {
            std::optional<VkExtent2D> out;

            for (auto& img : src.output_atta_) {
                auto src_img = images.find(img->name());
                const auto this_extent = src_img->extent2d();

                if (out) {
                    if (out->width != this_extent.width ||
                        out->height != this_extent.height) {
                        MIRINAE_ABORT(
                            "Render graph pass '{}' has inconsistent image "
                            "extent",
                            src.name()
                        );
                    }
                } else {
                    out = this_extent;
                }
            }

            MIRINAE_ASSERT(out.has_value());
            return out.value();
        }

        struct FrameData {
            mirinae::Fbuf fbuf_;
            std::vector<mirinae::ImageMemoryBarrier> pre_barrier_;
            std::vector<mirinae::ImageMemoryBarrier> post_barrier_;
        };

    public:
        ::RenderGraphPass def_;
        mirinae::RenderPass render_pass_;
        std::vector<FrameData> frame_data_;
        std::vector<VkClearValue> clear_values_;
        std::unique_ptr<mirinae::rg::IRenderPassImpl> rpimpl_;
        VkExtent2D fbuf_extent_;
    };


    class RenderGraph : public mirinae::rg::IRenderGraph {

    public:
        RenderGraph(mirinae::VulkanDevice& device) : device_(device) {}

        ~RenderGraph() override { this->destroy(); }

        void record(const mirinae::RpContext& ctxt) override {
            for (auto& pass : passes_) {
                if (pass.rpimpl_) {
                    pass.rpimpl_->record(pass, ctxt);
                }
            }
        }

        mirinae::rg::IRenderGraph::IImage* get_img(
            std::string_view name
        ) override {
            if (auto img = images_.find(name)) {
                return img;
            }

            MIRINAE_ABORT("Render graph image '{}' not found", name);
            return nullptr;
        }

        mirinae::rg::IRenderGraph::IRenderPass* get_pass(
            std::string_view name
        ) override {
            for (auto& pass : passes_) {
                if (pass.name() == name) {
                    return &pass;
                }
            }

            MIRINAE_ABORT("Render graph pass '{}' not found", name);
            return nullptr;
        }

        void destroy() {
            for (auto& x : images_) {
                x.destroy(device_);
            }

            for (auto& x : passes_) {
                x.destroy(device_);
            }
        }

        void add_img(
            const mirinae::rg::RenderGraphImage& src,
            const uint32_t max_frames_in_flight,
            const uint32_t swhain_width,
            const uint32_t swhain_height
        ) {
            images_.append(
                src, max_frames_in_flight, swhain_width, swhain_height, device_
            );
        }

        void add_pass(
            const ::RenderGraphPass& src,
            const uint32_t max_frames_in_flight,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts
        ) {
            passes_.emplace_back(src, max_frames_in_flight, images_, device_);
            auto& pass = passes_.back();

            if (pass.rpimpl_) {
                pass.rpimpl_->init(pass.name(), *this, desclayouts, device_);
            }
        }

    private:
        mirinae::VulkanDevice& device_;
        std::deque<RgPass> passes_;
        ::NamedItemRegistry<RgImage> images_;
    };

}  // namespace


// Aux for RenderGraphDef
namespace {

    class RgImageReg {

    public:
        mirinae::rg::RenderGraphImage* find(const std::string_view name) {
            for (auto& img : images_) {
                if (img.name() == name) {
                    return &img;
                }
            }
            return nullptr;
        }

        mirinae::rg::RenderGraphImage& create(const std::string_view name) {
            if (auto img = this->find(name)) {
                MIRINAE_ABORT("Render graph image '{}' already exists", name);
            }

            images_.emplace_back(name);
            return images_.back();
        }

        auto begin() const { return images_.begin(); }
        auto end() const { return images_.end(); }

    private:
        std::deque<mirinae::rg::RenderGraphImage> images_;
    };


    class RgPassReg {

    public:
        ::RenderGraphPass* find(const std::string_view name) {
            for (auto& pass : passes_) {
                if (pass.name() == name) {
                    return &pass;
                }
            }
            return nullptr;
        }

        ::RenderGraphPass* find(const void* ptr) {
            for (auto& pass : passes_) {
                if (&pass == ptr) {
                    return &pass;
                }
            }
            return nullptr;
        }

        ::RenderGraphPass& create(const std::string_view name) {
            if (auto pass = this->find(name)) {
                MIRINAE_ABORT("Render graph pass '{}' already exists", name);
            }

            passes_.emplace_back(name);
            return passes_.back();
        }

        auto begin() const { return passes_.begin(); }
        auto end() const { return passes_.end(); }

    private:
        std::deque<::RenderGraphPass> passes_;
    };

}  // namespace


// RenderGraphDef
namespace mirinae::rg {

    class RenderGraphDef::Impl {

    public:
        auto& images() { return images_; }
        auto& passes() { return passes_; }

        std::unique_ptr<IRenderGraph> build(
            RpResources& rp_res,
            DesclayoutManager& desclayouts,
            Swapchain& swhain,
            VulkanDevice& device
        ) {
            auto out = std::make_unique<RenderGraph>(device);

            for (auto& img : images_) {
                out->add_img(
                    img,
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    swhain.width(),
                    swhain.height()
                );
            }

            for (auto& pass : passes_) {
                out->add_pass(
                    pass, mirinae::MAX_FRAMES_IN_FLIGHT, rp_res, desclayouts
                );
            }

            for (auto& pass : passes_) {
                for (auto& out_img : pass.output_atta_) {
                    mirinae::ImageMemoryBarrier barrier;
                    barrier.set_src_access(0)
                        .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                        .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                        .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                        .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                        .set_signle_mip_layer();


                }

                break;
            }

            mirinae::ImageMemoryBarrier barrier;
            barrier.set_src_access(0)
                .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_signle_mip_layer();

            return out;
        }

    private:
        ::RgImageReg images_;
        ::RgPassReg passes_;
    };


    RenderGraphDef::RenderGraphDef() : pimpl_(std::make_unique<Impl>()) {}

    RenderGraphDef::~RenderGraphDef() = default;

    RenderGraphImage& RenderGraphDef::new_img(const std::string_view name) {
        MIRINAE_GET_IMPL();
        return pimpl.images().create(name);
    }

    RenderGraphImage& RenderGraphDef::get_img(const std::string_view name) {
        MIRINAE_GET_IMPL();
        if (auto img = pimpl.images().find(name))
            return *img;
        else
            MIRINAE_ABORT("Render graph image '{}' not found", name);
    }

    IRenderGraphPass& RenderGraphDef::new_pass(const std::string_view name) {
        MIRINAE_GET_IMPL();
        return pimpl.passes().create(name);
    }

    std::unique_ptr<IRenderGraph> RenderGraphDef::build(
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        Swapchain& swhain,
        VulkanDevice& device
    ) {
        MIRINAE_GET_IMPL();
        return pimpl.build(rp_res, desclayouts, swhain, device);
    }

}  // namespace mirinae::rg
