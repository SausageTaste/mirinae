#include "mirinae/render/renderpass.hpp"

#include <stdexcept>

#include <spdlog/spdlog.h>

#include "mirinae/render/vkmajorplayers.hpp"


// Builders
namespace {

    class DescLayoutBuilder {

    public:
        void add_uniform_buffer(VkShaderStageFlagBits stage_flags, uint32_t count) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++uniform_buffer_count_;
        }

        void add_combined_image_sampler(VkShaderStageFlagBits stage_flags, uint32_t count) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++combined_image_sampler_count_;
        }

        std::optional<VkDescriptorSetLayout> build(VkDevice logi_device) const {
            VkDescriptorSetLayoutCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            create_info.bindingCount = static_cast<uint32_t>(bindings_.size());
            create_info.pBindings = bindings_.data();

            VkDescriptorSetLayout handle;
            if (VK_SUCCESS != vkCreateDescriptorSetLayout(logi_device, &create_info, nullptr, &handle))
                return std::nullopt;

            return handle;
        }

        auto ubuf_count() const { return uniform_buffer_count_; }
        auto img_sampler_count() const { return combined_image_sampler_count_; }

    public:
        std::vector<VkDescriptorSetLayoutBinding> bindings_;
        size_t uniform_buffer_count_ = 0;
        size_t combined_image_sampler_count_ = 0;

    };


    class AttachmentDescBuilder {

    public:
        const VkAttachmentDescription* data() const {
            return attachments_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(attachments_.size());
        }

        VkAttachmentDescription& add(
            const VkFormat format,
            const VkImageLayout final_layout,
            const VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            const VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
            const VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE,
            const VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            const VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT
        ) {
            auto& added = attachments_.emplace_back();
            added = {};

            added.format = format;
            added.samples = samples;
            added.loadOp = load_op;
            added.storeOp = store_op;
            added.stencilLoadOp = stencil_load_op;
            added.stencilStoreOp = stencil_store_op;
            added.initialLayout = initial_layout;
            added.finalLayout = final_layout;

            return added;
        }

    private:
        std::vector<VkAttachmentDescription> attachments_;

    };


    class AttachmentRefBuilder {

    public:
        const VkAttachmentReference* data() const {
            return attachments_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(attachments_.size());
        }

        void add(uint32_t index, VkImageLayout layout) {
            auto& added = attachments_.emplace_back();
            added = {};

            added.attachment = index;
            added.layout = layout;
        }

    private:
        std::vector<VkAttachmentReference> attachments_;

    };


    class ShaderModule {

    public:
        ShaderModule(const mirinae::respath_t& spv_path, mirinae::VulkanDevice& device)
            : device_{ device }
        {
            if (auto spv = device.filesys().read_file_to_vector(spv_path)) {
                if (auto shader = this->create_shader_module(spv.value(), device.logi_device())) {
                    handle_ = shader.value();
                }
                else {
                    throw std::runtime_error{ fmt::format("Failed to create shader module with given data: {}", spv_path) };
                }
            }
            else {
                throw std::runtime_error{ fmt::format("Failed to read a shader file: {}", spv_path) };
            }
        }

        ~ShaderModule() {
            if (VK_NULL_HANDLE != handle_) {
                vkDestroyShaderModule(device_.logi_device(), handle_, nullptr);
                handle_ = VK_NULL_HANDLE;
            }
        }

        VkShaderModule get() {
            return handle_;
        }

    private:
        static std::optional<VkShaderModule> create_shader_module(const std::vector<uint8_t>& spv, VkDevice logi_device) {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = spv.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());

            VkShaderModule shaderModule;
            if (VK_SUCCESS != vkCreateShaderModule(logi_device, &createInfo, nullptr, &shaderModule))
                return std::nullopt;

            return shaderModule;
        }

        mirinae::VulkanDevice& device_;
        VkShaderModule handle_ = VK_NULL_HANDLE;

    };


    class ColorBlendAttachmentStateBuilder {

    public:
        const VkPipelineColorBlendAttachmentState* data() const {
            return data_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(data_.size());
        }

        void add() {
            auto& added = data_.emplace_back();
            added.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            added.blendEnable = VK_FALSE;
            added.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            added.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            added.colorBlendOp = VK_BLEND_OP_ADD;
            added.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            added.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            added.alphaBlendOp = VK_BLEND_OP_ADD;
        }

    private:
        std::vector<VkPipelineColorBlendAttachmentState> data_;

    };

}


// Unorthodox
namespace { namespace unorthodox {

    VkDescriptorSetLayout create_desclayout_model(mirinae::VulkanDevice& device) {
        DescLayoutBuilder builder;
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);

        if (auto handle = builder.build(device.logi_device()))
            return handle.value();
        else
            throw std::runtime_error("Failed to create descriptor set layout: model");
    }

    VkDescriptorSetLayout create_desclayout_actor(mirinae::VulkanDevice& device) {
        DescLayoutBuilder builder;
        builder.add_uniform_buffer(VK_SHADER_STAGE_VERTEX_BIT, 1);

        if (auto handle = builder.build(device.logi_device()))
            return handle.value();
        else
            throw std::runtime_error("Failed to create descriptor set layout: actor");
    }

    VkRenderPass create_renderpass(VkFormat swapchain_format, VkFormat albedo_format, VkFormat normal_format, VkFormat depth_format, VkDevice logi_device) {
        ::AttachmentDescBuilder attachments;
        attachments.add(swapchain_format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        attachments.add(depth_format, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        attachments.add(albedo_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        attachments.add(normal_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        std::vector<VkSubpassDescription> subpasses;
        const VkAttachmentReference depth_attachment_ref{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        // First subpass
        // ---------------------------------------------------------------------------------

        ::AttachmentRefBuilder color_refs_1;
        color_refs_1.add(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // albedo
        color_refs_1.add(3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // normal

        subpasses.emplace_back();
        subpasses.back().pipelineBindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses.back().pDepthStencilAttachment    = &depth_attachment_ref;
        subpasses.back().colorAttachmentCount       = color_refs_1.size();
        subpasses.back().pColorAttachments          = color_refs_1.data();
        subpasses.back().inputAttachmentCount       = 0;
        subpasses.back().pInputAttachments          = nullptr;

        // Second subpass
        // ---------------------------------------------------------------------------------

        ::AttachmentRefBuilder color_refs_2;
        color_refs_1.add(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // swapchain

        ::AttachmentRefBuilder input_refs_2;
        input_refs_2.add(1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);  // depth
        input_refs_2.add(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);  // albedo
        input_refs_2.add(3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);  // normal

        subpasses.emplace_back();
        subpasses.back().pipelineBindPoint          = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses.back().pDepthStencilAttachment    = nullptr;
        subpasses.back().colorAttachmentCount       = color_refs_2.size();
        subpasses.back().pColorAttachments          = color_refs_2.data();
        subpasses.back().inputAttachmentCount       = input_refs_2.size();
        subpasses.back().pInputAttachments          = input_refs_2.data();

        // Dependencies
        // ---------------------------------------------------------------------------------

        std::vector<VkSubpassDependency> dependencies;

        dependencies.emplace_back();
        dependencies.back().srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies.back().dstSubpass      = 0;
        dependencies.back().srcStageMask    = 0;
        dependencies.back().srcAccessMask   = 0;
        dependencies.back().dstStageMask    = 0;
        dependencies.back().dstAccessMask   = 0;
        dependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies.emplace_back();
        dependencies.back().srcSubpass      = 0;
        dependencies.back().dstSubpass      = 1;
        dependencies.back().srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.back().srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies.back().dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies.back().dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
        dependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies.emplace_back();
        dependencies.back().srcSubpass      = 1;
        dependencies.back().dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies.back().srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.back().srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies.back().dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies.back().dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
        dependencies.back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Create render pass
        // ---------------------------------------------------------------------------------

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments    = attachments.data();
        create_info.subpassCount    = static_cast<uint32_t>(subpasses.size());
        create_info.pSubpasses      = subpasses.data();
        create_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
        create_info.pDependencies   = dependencies.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS != vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_model,
        VkDescriptorSetLayout desclayout_actor,
        mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_model,
            desclayout_actor,
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(desclayouts.size());
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device.logi_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        return pipelineLayout;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderModule vert_shader{ "asset/spv/unorthodox_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/unorthodox_frag.spv", device };

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        {
            auto& shader_info = shaderStages.emplace_back();
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_info.module = vert_shader.get();
            shader_info.pName = "main";
        }
        {
            auto& shader_info = shaderStages.emplace_back();
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_info.module = frag_shader.get();
            shader_info.pName = "main";
        }

        std::vector<VkDynamicState> dynamicStates{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        {
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();
        }

        auto binding_description = mirinae::make_vertex_static_binding_description();
        auto attribute_descriptions = mirinae::make_vertex_static_attribute_descriptions();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        {
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &binding_description;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
            vertexInputInfo.pVertexAttributeDescriptions = attribute_descriptions.data();
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        {
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;
        }

        VkPipelineViewportStateCreateInfo viewportState{};
        {
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;
        }

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        {
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0;
            rasterizer.depthBiasClamp = 0;
            rasterizer.depthBiasSlopeFactor = 0;
        }

        VkPipelineMultisampleStateCreateInfo multisampling{};
        {
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampling.minSampleShading = 1;
            multisampling.pSampleMask = nullptr;
            multisampling.alphaToCoverageEnable = VK_FALSE;
            multisampling.alphaToOneEnable = VK_FALSE;
        }

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        {
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.minDepthBounds = 0;
            depthStencil.maxDepthBounds = 0;
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {};
            depthStencil.back = {};
        }

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add();
        color_blend_attachment_states.add();
        color_blend_attachment_states.add();

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        {
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = color_blend_attachment_states.size();
            colorBlending.pAttachments = color_blend_attachment_states.data();
            colorBlending.blendConstants[0] = 0;
            colorBlending.blendConstants[1] = 0;
            colorBlending.blendConstants[2] = 0;
            colorBlending.blendConstants[3] = 0;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderpass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(device.logi_device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        return graphicsPipeline;
    }


    class RenderPassBundle : public mirinae::IRenderPassBundle {

    public:
        RenderPassBundle(VkFormat swapchain_format, VkFormat depthmap_format, mirinae::VulkanDevice& device)
            : device_(device)
        {
            formats_ = {
                swapchain_format,
                depthmap_format,
                VK_FORMAT_R8G8B8A8_UNORM,  // albedo
                VK_FORMAT_R8G8B8A8_UNORM,  // normal
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,  // composition
            };

            desclayouts_.push_back(::unorthodox::create_desclayout_model(device));
            desclayouts_.push_back(::unorthodox::create_desclayout_actor(device));
            renderpass_ = ::unorthodox::create_renderpass(
                formats_.at(0),
                formats_.at(2),
                formats_.at(3),
                formats_.at(1),
                device.logi_device()
            );
            layout_ = ::unorthodox::create_pipeline_layout(
                desclayouts_.at(0),
                desclayouts_.at(1),
                device
            );
            pipeline_ = ::unorthodox::create_pipeline(
                renderpass_,
                layout_,
                device
            );
        }

        ~RenderPassBundle() {
            this->destroy();
        }

        void destroy() {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(device_.logi_device(), layout_, nullptr);
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(device_.logi_device(), renderpass_, nullptr);
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : desclayouts_) {
                vkDestroyDescriptorSetLayout(device_.logi_device(), handle, nullptr);
            }
            desclayouts_.clear();

            formats_.clear();
        }

    private:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> desclayouts_;
        std::vector<VkFormat> formats_;

    };

}}


namespace mirinae {

    std::unique_ptr<IRenderPassBundle> create_unorthodox(VkFormat swapchain_format, VkFormat depthmap_format, VulkanDevice& device) {
        return std::make_unique<::unorthodox::RenderPassBundle>(swapchain_format, depthmap_format, device);
    }

}
