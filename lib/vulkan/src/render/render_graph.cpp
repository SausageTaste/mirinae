#include "mirinae/render/render_graph.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"

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

}  // namespace


// RenderGraphImage
namespace mirinae::rg {

    const std::string& RenderGraphImage::name() const { return name_; }

    ImageType RenderGraphImage::deduce_type() const {
        return ::classify_img_format(format_);
    }

    RenderGraphImage& RenderGraphImage::set_format(VkFormat x) {
        format_ = x;
        return *this;
    }

    RenderGraphImage& RenderGraphImage::set_type(ImageType x) {
        type_ = x;
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

    private:
        std::deque<mirinae::rg::RenderGraphImage> images_;
    };


    class RgPassReg {

    public:
        mirinae::rg::RenderGraphPass* find(const std::string_view name) {
            for (auto& pass : passes_) {
                if (pass.name() == name) {
                    return &pass;
                }
            }
            return nullptr;
        }

        mirinae::rg::RenderGraphPass& create(const std::string_view name) {
            if (auto pass = this->find(name)) {
                MIRINAE_ABORT("Render graph pass '{}' already exists", name);
            }

            passes_.emplace_back(name);
            return passes_.back();
        }

    private:
        std::deque<mirinae::rg::RenderGraphPass> passes_;
    };

}  // namespace


// RenderGraphDef
namespace mirinae::rg {

    class RenderGraphDef::Impl {

    public:
        auto& images() { return images_; }
        auto& passes() { return passes_; }

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

    RenderGraphPass& RenderGraphDef::new_pass(const std::string_view name) {
        MIRINAE_GET_IMPL();
        return pimpl.passes().create(name);
    }

}  // namespace mirinae::rg
