#include "mirinae/render/renderpass/common.hpp"


// RenderPassBuilder
namespace mirinae {

    VkRenderPass RenderPassBuilder::build(VkDevice logi_device) {
        VkSubpassDescription subpass_desc{};
        subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_desc.colorAttachmentCount = color_attach_ref_.size();
        subpass_desc.pColorAttachments = color_attach_ref_.data();
        subpass_desc.pDepthStencilAttachment = depth_attach_ref_.data();

        VkRenderPassCreateInfo cinfo{};
        cinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        cinfo.attachmentCount = attach_desc_.size();
        cinfo.pAttachments = attach_desc_.data();
        cinfo.subpassCount = 1;
        cinfo.pSubpasses = &subpass_desc;
        cinfo.dependencyCount = subpass_dep_.size();
        cinfo.pDependencies = subpass_dep_.data();

        VkRenderPass output = VK_NULL_HANDLE;
        const auto res = vkCreateRenderPass(logi_device, &cinfo, NULL, &output);
        if (VK_SUCCESS != res) {
            throw std::runtime_error("failed to create render pass.");
        }

        return output;
    }

}  // namespace mirinae
