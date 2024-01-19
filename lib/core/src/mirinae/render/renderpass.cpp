#include "mirinae/render/renderpass.hpp"

#include <stdexcept>

#include <spdlog/spdlog.h>

#include "mirinae/render/vkmajorplayers.hpp"


// Builders
namespace {

    VkVertexInputBindingDescription make_vertex_static_binding_description() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(mirinae::VertexStatic);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription> make_vertex_static_attribute_descriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 0;
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(mirinae::VertexStatic, pos_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 1;
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(mirinae::VertexStatic, normal_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 2;
            description.format = VK_FORMAT_R32G32_SFLOAT;
            description.offset = offsetof(mirinae::VertexStatic, texcoord_);
        }

        return attributeDescriptions;
    }

    VkFramebuffer create_framebuffer(
        uint32_t width,
        uint32_t height,
        VkRenderPass renderpass,
        VkDevice logi_device,
        const std::vector<VkImageView>& attachments
    ) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderpass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        VkFramebuffer output = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(logi_device, &framebufferInfo, nullptr, &output) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }

        return output;
    }


    class DescLayoutBuilder {

    public:
        DescLayoutBuilder(const char* name) : name_{ name } {}

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

        void add_input_attachment(VkShaderStageFlagBits stage_flags, uint32_t count) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++input_attachment_count_;
        }

        VkDescriptorSetLayout build(VkDevice logi_device) const {
            VkDescriptorSetLayoutCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            create_info.bindingCount = static_cast<uint32_t>(bindings_.size());
            create_info.pBindings = bindings_.data();

            VkDescriptorSetLayout handle;
            if (VK_SUCCESS != vkCreateDescriptorSetLayout(logi_device, &create_info, nullptr, &handle))
                throw std::runtime_error{ fmt::format("Failed to create descriptor set layout: {}", name_) };

            return handle;
        }

        VkDescriptorSetLayout build_in_place(mirinae::DesclayoutManager& desclayouts, VkDevice logi_device) {
            auto handle = this->build(logi_device);
            desclayouts.add(name_, handle);
            return handle;
        }

        auto& name() const { return name_; }
        auto ubuf_count() const { return uniform_buffer_count_; }
        auto img_sampler_count() const { return combined_image_sampler_count_; }

    public:
        std::string name_;
        std::vector<VkDescriptorSetLayoutBinding> bindings_;
        size_t uniform_buffer_count_ = 0;
        size_t combined_image_sampler_count_ = 0;
        size_t input_attachment_count_ = 0;

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

        VkShaderModule get() const {
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


    std::array<VkPipelineShaderStageCreateInfo, 2> create_info_shader_stages_pair(const ShaderModule& vertex, const ShaderModule& fragment) {
        std::array<VkPipelineShaderStageCreateInfo, 2> output{};
        {
            auto& shader_info = output[0];
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_info.module = vertex.get();
            shader_info.pName = "main";
        }
        {
            auto& shader_info = output[1];
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_info.module = fragment.get();
            shader_info.pName = "main";
        }
        return output;
    }

}


// Pipeline builders
namespace {

    class ColorBlendAttachmentStateBuilder {

    public:
        const VkPipelineColorBlendAttachmentState* data() const {
            return data_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(data_.size());
        }

        template <bool TBlendingEnabled>
        void add() {
            auto& added = data_.emplace_back();
            added.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            added.colorBlendOp = VK_BLEND_OP_ADD;
            added.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            added.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            added.alphaBlendOp = VK_BLEND_OP_ADD;

            if constexpr (TBlendingEnabled) {
                added.blendEnable = VK_TRUE;
                added.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                added.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            }
            else {
                added.blendEnable = VK_FALSE;
                added.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                added.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            }
        }

    private:
        std::vector<VkPipelineColorBlendAttachmentState> data_;

    };

}


// gbuf
namespace { namespace gbuf {

    VkDescriptorSetLayout create_desclayout_model(mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device) {
        DescLayoutBuilder builder{ "gbuf:model" };
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkDescriptorSetLayout create_desclayout_actor(mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device) {
        DescLayoutBuilder builder{ "gbuf:actor" };
        builder.add_uniform_buffer(VK_SHADER_STAGE_VERTEX_BIT, 1);
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat depth_format, VkFormat albedo_format, VkFormat normal_format, VkDevice logi_device) {
        ::AttachmentDescBuilder attachments;
        attachments.add(depth_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        attachments.add(albedo_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        attachments.add(normal_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // albedo
        color_attachment_refs.add(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // normal

        const VkAttachmentReference depth_attachment_ref{ 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = 1;
        create_info.pDependencies = &dependency;

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
        ::ShaderModule vert_shader{ "asset/spv/gbuf_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/gbuf_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(vert_shader, frag_shader);

        std::vector<VkDynamicState> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_info{};
        {
            dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
            dynamic_state_info.pDynamicStates = dynamic_states.data();
        }

        auto binding_description = ::make_vertex_static_binding_description();
        auto attribute_descriptions = ::make_vertex_static_attribute_descriptions();
        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        {
            vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_info.vertexBindingDescriptionCount = 1;
            vertex_input_info.pVertexBindingDescriptions = &binding_description;
            vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
            vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        {
            input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;
        }

        VkPipelineViewportStateCreateInfo viewport_state{};
        {
            viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state.viewportCount = 1;
            viewport_state.scissorCount = 1;
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

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        {
            depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil.depthTestEnable = VK_TRUE;
            depth_stencil.depthWriteEnable = VK_TRUE;
            depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depth_stencil.depthBoundsTestEnable = VK_FALSE;
            depth_stencil.minDepthBounds = 0;
            depth_stencil.maxDepthBounds = 0;
            depth_stencil.stencilTestEnable = VK_FALSE;
            depth_stencil.front = {};
            depth_stencil.back = {};
        }

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<false>();
        color_blend_attachment_states.add<false>();

        VkPipelineColorBlendStateCreateInfo color_blending{};
        {
            color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blending.logicOpEnable = VK_FALSE;
            color_blending.logicOp = VK_LOGIC_OP_COPY;
            color_blending.attachmentCount = color_blend_attachment_states.size();
            color_blending.pAttachments = color_blend_attachment_states.data();
            color_blending.blendConstants[0] = 0;
            color_blending.blendConstants[1] = 0;
            color_blending.blendConstants[2] = 0;
            color_blending.blendConstants[3] = 0;
        }

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.layout = pipelineLayout;
        pipeline_info.renderPass = renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

        VkPipeline graphics_pipeline;
        if (vkCreateGraphicsPipelines(device.logi_device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        return graphics_pipeline;
    }


    class RenderPassBundle : public mirinae::IRenderPassBundle {

    public:
        RenderPassBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : device_(device)
        {
            formats_ = {
                fbuf_bundle.depth().format(),
                fbuf_bundle.albedo().format(),
                fbuf_bundle.normal().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                formats_.at(1),
                formats_.at(2),
                device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_model(desclayouts, device),
                create_desclayout_actor(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(
                renderpass_,
                layout_,
                device
            );

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    width,
                    height,
                    renderpass_,
                    device.logi_device(),
                    {
                        fbuf_bundle.depth().image_view(),
                        fbuf_bundle.albedo().image_view(),
                        fbuf_bundle.normal().image_view(),
                    }
                ));
            }
        }

        ~RenderPassBundle() override {
            this->destroy();
        }

        void destroy() override {
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

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override {
            return renderpass_;
        }

        VkPipeline pipeline() override {
            return pipeline_;
        }

        VkPipelineLayout pipeline_layout() override {
            return layout_;
        }

        VkFramebuffer fbuf_at(uint32_t index) override {
            return fbufs_.at(index);
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        std::array<VkFormat, 3> formats_;
        std::array<VkClearValue, 3> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images

    };

}}


// composition
namespace { namespace composition {

    VkDescriptorSetLayout create_desclayout_main(mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device) {
        DescLayoutBuilder builder{ "composition:main" };
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // depth
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // albedo
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // normal
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat composition_format, VkDevice logi_device) {
        ::AttachmentDescBuilder attachments;
        attachments.add(composition_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // composition

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = nullptr;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = 1;
        create_info.pDependencies = &dependency;

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS != vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_main,
        mirinae::VulkanDevice& device
    ) {
        const std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_main,
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
        ::ShaderModule vert_shader{ "asset/spv/composition_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/composition_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(vert_shader, frag_shader);

        std::vector<VkDynamicState> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_info{};
        {
            dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
            dynamic_state_info.pDynamicStates = dynamic_states.data();
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        {
            vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_info.vertexBindingDescriptionCount = 0;
            vertex_input_info.pVertexBindingDescriptions = nullptr;
            vertex_input_info.vertexAttributeDescriptionCount = 0;
            vertex_input_info.pVertexAttributeDescriptions = nullptr;
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        {
            input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;
        }

        VkPipelineViewportStateCreateInfo viewport_state{};
        {
            viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state.viewportCount = 1;
            viewport_state.scissorCount = 1;
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

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        {
            depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil.depthTestEnable = VK_TRUE;
            depth_stencil.depthWriteEnable = VK_TRUE;
            depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depth_stencil.depthBoundsTestEnable = VK_FALSE;
            depth_stencil.minDepthBounds = 0;
            depth_stencil.maxDepthBounds = 0;
            depth_stencil.stencilTestEnable = VK_FALSE;
            depth_stencil.front = {};
            depth_stencil.back = {};
        }

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<false>();

        VkPipelineColorBlendStateCreateInfo color_blending{};
        {
            color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blending.logicOpEnable = VK_FALSE;
            color_blending.logicOp = VK_LOGIC_OP_COPY;
            color_blending.attachmentCount = color_blend_attachment_states.size();
            color_blending.pAttachments = color_blend_attachment_states.data();
            color_blending.blendConstants[0] = 0;
            color_blending.blendConstants[1] = 0;
            color_blending.blendConstants[2] = 0;
            color_blending.blendConstants[3] = 0;
        }

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.layout = pipelineLayout;
        pipeline_info.renderPass = renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

        VkPipeline graphics_pipeline;
        if (vkCreateGraphicsPipelines(device.logi_device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        return graphics_pipeline;
    }


    class RenderPassBundle : public mirinae::IRenderPassBundle {

    public:
        RenderPassBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : device_(device)
        {
            formats_ = {
                fbuf_bundle.composition().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_main(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(
                renderpass_,
                layout_,
                device
            );

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    width,
                    height,
                    renderpass_,
                    device.logi_device(),
                    {
                        fbuf_bundle.composition().image_view(),
                    }
                ));
            }
        }

        ~RenderPassBundle() override {
            this->destroy();
        }

        void destroy() override {
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

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override {
            return renderpass_;
        }

        VkPipeline pipeline() override {
            return pipeline_;
        }

        VkPipelineLayout pipeline_layout() override {
            return layout_;
        }

        VkFramebuffer fbuf_at(uint32_t index) override {
            return fbufs_.at(index);
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images

    };

}}


// fillscreen
namespace { namespace fillscreen {

    VkDescriptorSetLayout create_desclayout_main(mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device) {
        DescLayoutBuilder builder{ "fillscreen:main" };
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // composition
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat surface, VkDevice logi_device) {
        ::AttachmentDescBuilder attachments;
        attachments.add(surface, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = nullptr;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = 1;
        create_info.pDependencies = &dependency;

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS != vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_main,
        mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_main,
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
        ::ShaderModule vert_shader{ "asset/spv/fill_screen_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/fill_screen_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(vert_shader, frag_shader);

        std::vector<VkDynamicState> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_info{};
        {
            dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
            dynamic_state_info.pDynamicStates = dynamic_states.data();
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        {
            vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_info.vertexBindingDescriptionCount = 0;
            vertex_input_info.pVertexBindingDescriptions = nullptr;
            vertex_input_info.vertexAttributeDescriptionCount = 0;
            vertex_input_info.pVertexAttributeDescriptions = nullptr;
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        {
            input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;
        }

        VkPipelineViewportStateCreateInfo viewport_state{};
        {
            viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state.viewportCount = 1;
            viewport_state.scissorCount = 1;
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

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        {
            depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil.depthTestEnable = VK_FALSE;
            depth_stencil.depthWriteEnable = VK_FALSE;
            depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depth_stencil.depthBoundsTestEnable = VK_FALSE;
            depth_stencil.minDepthBounds = 0;
            depth_stencil.maxDepthBounds = 0;
            depth_stencil.stencilTestEnable = VK_FALSE;
            depth_stencil.front = {};
            depth_stencil.back = {};
        }

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<false>();

        VkPipelineColorBlendStateCreateInfo color_blending{};
        {
            color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blending.logicOpEnable = VK_FALSE;
            color_blending.logicOp = VK_LOGIC_OP_COPY;
            color_blending.attachmentCount = color_blend_attachment_states.size();
            color_blending.pAttachments = color_blend_attachment_states.data();
            color_blending.blendConstants[0] = 0;
            color_blending.blendConstants[1] = 0;
            color_blending.blendConstants[2] = 0;
            color_blending.blendConstants[3] = 0;
        }

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.layout = pipelineLayout;
        pipeline_info.renderPass = renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

        VkPipeline graphics_pipeline;
        if (vkCreateGraphicsPipelines(device.logi_device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        return graphics_pipeline;
    }


    class RenderPassBundle : public mirinae::IRenderPassBundle {

    public:
        RenderPassBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : device_(device)
        {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_main(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(
                renderpass_,
                layout_,
                device
            );

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    width,
                    height,
                    renderpass_,
                    device.logi_device(),
                    {
                        swapchain.view_at(i),
                    }
                ));
            }
        }

        ~RenderPassBundle() override {
            this->destroy();
        }

        void destroy() override {
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

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override {
            return renderpass_;
        }

        VkPipeline pipeline() override {
            return pipeline_;
        }

        VkPipelineLayout pipeline_layout() override {
            return layout_;
        }

        VkFramebuffer fbuf_at(uint32_t index) override {
            return fbufs_.at(index);
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images

    };

}}


// overlay
namespace { namespace overlay {

    VkDescriptorSetLayout create_desclayout_main(mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device) {
        DescLayoutBuilder builder{ "overlay:main" };
        builder.add_uniform_buffer(VK_SHADER_STAGE_VERTEX_BIT, 1);  // U_OverlayMain
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // color
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // mask
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat surface, VkDevice logi_device) {
        ::AttachmentDescBuilder attachments;
        attachments.add(surface,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ATTACHMENT_LOAD_OP_LOAD,
            VK_ATTACHMENT_STORE_OP_STORE
        );

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = nullptr;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = 1;
        create_info.pDependencies = &dependency;

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS != vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_main,
        mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_main,
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
        ::ShaderModule vert_shader{ "asset/spv/overlay_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/overlay_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(vert_shader, frag_shader);

        std::vector<VkDynamicState> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_info{};
        {
            dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
            dynamic_state_info.pDynamicStates = dynamic_states.data();
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        {
            vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_info.vertexBindingDescriptionCount = 0;
            vertex_input_info.pVertexBindingDescriptions = nullptr;
            vertex_input_info.vertexAttributeDescriptionCount = 0;
            vertex_input_info.pVertexAttributeDescriptions = nullptr;
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        {
            input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;
        }

        VkPipelineViewportStateCreateInfo viewport_state{};
        {
            viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state.viewportCount = 1;
            viewport_state.scissorCount = 1;
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

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        {
            depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil.depthTestEnable = VK_FALSE;
            depth_stencil.depthWriteEnable = VK_FALSE;
            depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depth_stencil.depthBoundsTestEnable = VK_FALSE;
            depth_stencil.minDepthBounds = 0;
            depth_stencil.maxDepthBounds = 0;
            depth_stencil.stencilTestEnable = VK_FALSE;
            depth_stencil.front = {};
            depth_stencil.back = {};
        }

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<true>();

        VkPipelineColorBlendStateCreateInfo color_blending{};
        {
            color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blending.logicOpEnable = VK_FALSE;
            color_blending.logicOp = VK_LOGIC_OP_COPY;
            color_blending.attachmentCount = color_blend_attachment_states.size();
            color_blending.pAttachments = color_blend_attachment_states.data();
            color_blending.blendConstants[0] = 0;
            color_blending.blendConstants[1] = 0;
            color_blending.blendConstants[2] = 0;
            color_blending.blendConstants[3] = 0;
        }

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.layout = pipelineLayout;
        pipeline_info.renderPass = renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

        VkPipeline graphics_pipeline;
        if (vkCreateGraphicsPipelines(device.logi_device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        return graphics_pipeline;
    }


    class RenderPassBundle : public mirinae::IRenderPassBundle {

    public:
        RenderPassBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : device_(device)
        {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_main(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(
                renderpass_,
                layout_,
                device
            );

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    width,
                    height,
                    renderpass_,
                    device.logi_device(),
                    {
                        swapchain.view_at(i),
                    }
                ));
            }
        }

        ~RenderPassBundle() override {
            this->destroy();
        }

        void destroy() override {
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

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override {
            return renderpass_;
        }

        VkPipeline pipeline() override {
            return pipeline_;
        }

        VkPipelineLayout pipeline_layout() override {
            return layout_;
        }

        VkFramebuffer fbuf_at(uint32_t index) override {
            return fbufs_.at(index);
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images

    };

}}


namespace mirinae {

    std::unique_ptr<IRenderPassBundle> create_gbuf(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        return std::make_unique<::gbuf::RenderPassBundle>(width, height, fbuf_bundle, desclayouts, swapchain, device);
    }

    std::unique_ptr<IRenderPassBundle> create_composition(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        return std::make_unique<::composition::RenderPassBundle>(width, height, fbuf_bundle, desclayouts, swapchain, device);
    }

    std::unique_ptr<IRenderPassBundle> create_fillscreen(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        return std::make_unique<::fillscreen::RenderPassBundle>(width, height, fbuf_bundle, desclayouts, swapchain, device);
    }

    std::unique_ptr<IRenderPassBundle> create_overlay(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        return std::make_unique<::overlay::RenderPassBundle>(width, height, fbuf_bundle, desclayouts, swapchain, device);
    }

}
