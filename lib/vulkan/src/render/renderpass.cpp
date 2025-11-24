#include "render/renderpass.hpp"

#include <stdexcept>

#include "mirinae/lightweight/include_spdlog.hpp"

#include "render/vkmajorplayers.hpp"
#include "renderpass/builder.hpp"
#include "renderpass/common.hpp"


// fillscreen
namespace { namespace fillscreen {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "fillscreen:main" };
        builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // compo
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat surface, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(surface)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();

        builder.color_attach_ref().add_color_attach(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/fill_screen_vert.spv")
            .add_frag(":asset/spv/fill_screen_frag.spv");

        builder.color_blend_state().add(false);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_frag_flag()
                          .pc<mirinae::U_FillScreenPushConst>(0)
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            fbufs_.resize(swapchain.views_count());
            for (int i = 0; i < swapchain.views_count(); ++i) {
                mirinae::FbufCinfo fbuf_cinfo;

                fbuf_cinfo.set_rp(renderpass_)
                    .set_dim(swapchain.extent())
                    .add_attach(swapchain.view_at(i));
                fbufs_.at(i).init(fbuf_cinfo.get(), device.logi_device());
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);

            for (auto& handle : fbufs_) {
                handle.destroy(device_.logi_device());
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return fbufs_.at(index).get();
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
        std::vector<mirinae::Fbuf> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::fillscreen


// overlay
namespace { namespace overlay {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "overlay:main" };
        builder
            .add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1)    // U_OverlayMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // color
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // mask
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat surface, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(surface)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .op_pair_load_store();

        builder.color_attach_ref().add_color_attach(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/overlay_vert.spv")
            .add_frag(":asset/spv/overlay_frag.spv");

        builder.color_blend_state().add(true);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            mirinae::PipelineLayoutBuilder{}
                .desc(create_desclayout_main(desclayouts, device))
                .add_vertex_flag()
                .add_frag_flag()
                .pc(0, sizeof(mirinae::U_OverlayPushConst))
                .build(layout_, device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            fbufs_.resize(swapchain.views_count());
            for (int i = 0; i < swapchain.views_count(); ++i) {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(renderpass_)
                    .set_dim(swapchain.extent())
                    .add_attach(swapchain.view_at(i));
                fbufs_.at(i).init(fbuf_cinfo.get(), device.logi_device());
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);

            for (auto& handle : fbufs_) {
                handle.destroy(device_.logi_device());
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return fbufs_.at(index).get();
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
        std::vector<mirinae::Fbuf> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::overlay


// RenderPassPackage
namespace mirinae {

    void RenderPassPackage::add(
        const std::string& name, std::unique_ptr<IRenderPassBundle>&& rp
    ) {
        if (data_.find(name) != data_.end()) {
            SPDLOG_WARN(
                "Render pass bundle already exists, replacing: '{}'", name
            );
        }

        data_[name] = std::move(rp);
    }

    void RenderPassPackage::init_render_passes(
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        data_["fillscreen"] = std::make_unique<::fillscreen::RPBundle>(
            fbuf_bundle, desclayouts, swapchain, device
        );
        data_["overlay"] = std::make_unique<::overlay::RPBundle>(
            fbuf_bundle, desclayouts, swapchain, device
        );
    }

    void RenderPassPackage::destroy() { data_.clear(); }

    const IRenderPassBundle& RenderPassPackage::get(
        const std::string& name
    ) const {
        auto it = data_.find(name);
        if (it == data_.end()) {
            MIRINAE_ABORT("Render pass bundle not found: '{}'", name);
        }
        return *it->second;
    }

}  // namespace mirinae
