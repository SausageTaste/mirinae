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

    auto make_vertex_static_attribute_descriptions() {
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
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(mirinae::VertexStatic, tangent_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 3;
            description.format = VK_FORMAT_R32G32_SFLOAT;
            description.offset = offsetof(mirinae::VertexStatic, texcoord_);
        }

        return attributeDescriptions;
    }


    VkVertexInputBindingDescription make_vertex_skinned_binding_description() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(mirinae::VertexSkinned);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    auto make_vertex_skinned_attribute_descriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 0;
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(mirinae::VertexSkinned, pos_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 1;
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(mirinae::VertexSkinned, normal_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 2;
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(mirinae::VertexSkinned, tangent_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 3;
            description.format = VK_FORMAT_R32G32_SFLOAT;
            description.offset = offsetof(mirinae::VertexSkinned, uv_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 4;
            description.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            description.offset = offsetof(
                mirinae::VertexSkinned, joint_weights_
            );
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 5;
            description.format = VK_FORMAT_R32G32B32A32_SINT;
            description.offset = offsetof(
                mirinae::VertexSkinned, joint_indices_
            );
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
        framebufferInfo.attachmentCount = static_cast<uint32_t>(
            attachments.size()
        );
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        VkFramebuffer output = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(
                logi_device, &framebufferInfo, nullptr, &output
            ) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }

        return output;
    }


    class DescLayoutBuilder {

    public:
        DescLayoutBuilder(const char* name) : name_{ name } {}

        // Add uniform buffer
        DescLayoutBuilder& add_ubuf(
            VkShaderStageFlagBits stage_flags, uint32_t count
        ) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++uniform_buffer_count_;
            return *this;
        }

        // Add combined image sampler
        DescLayoutBuilder& add_img(
            VkShaderStageFlagBits stage_flags, uint32_t count
        ) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++combined_image_sampler_count_;
            return *this;
        }

        // Add input attachment
        DescLayoutBuilder& add_input_att(
            VkShaderStageFlagBits stage_flags, uint32_t count
        ) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++input_attachment_count_;
            return *this;
        }

        VkDescriptorSetLayout build(VkDevice logi_device) const {
            VkDescriptorSetLayoutCreateInfo create_info = {};
            create_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            create_info.bindingCount = static_cast<uint32_t>(bindings_.size());
            create_info.pBindings = bindings_.data();

            VkDescriptorSetLayout handle;
            if (vkCreateDescriptorSetLayout(
                    logi_device, &create_info, nullptr, &handle
                ) != VK_SUCCESS) {
                throw std::runtime_error{ fmt::format(
                    "Failed to create descriptor set layout: {}", name_
                ) };
            }

            return handle;
        }

        VkDescriptorSetLayout build_in_place(
            mirinae::DesclayoutManager& desclayouts, VkDevice logi_device
        ) {
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
        class ThinView {

        public:
            ThinView(VkAttachmentDescription& desc) : desc_{ desc } {}

            ~ThinView() = default;

            ThinView& format(VkFormat format) {
                desc_.format = format;
                return *this;
            }

            ThinView& samples(VkSampleCountFlagBits samples) {
                desc_.samples = samples;
                return *this;
            }

            ThinView& load_op(VkAttachmentLoadOp load_op) {
                desc_.loadOp = load_op;
                return *this;
            }

            ThinView& store_op(VkAttachmentStoreOp store_op) {
                desc_.storeOp = store_op;
                return *this;
            }

            ThinView& stencil_load_op(VkAttachmentLoadOp stencil_load_op) {
                desc_.stencilLoadOp = stencil_load_op;
                return *this;
            }

            ThinView& stencil_store_op(VkAttachmentStoreOp stencil_store_op) {
                desc_.stencilStoreOp = stencil_store_op;
                return *this;
            }

            ThinView& initial_layout(VkImageLayout initial_layout) {
                desc_.initialLayout = initial_layout;
                return *this;
            }

            ThinView& final_layout(VkImageLayout final_layout) {
                desc_.finalLayout = final_layout;
                return *this;
            }

        private:
            VkAttachmentDescription& desc_;
        };

        const VkAttachmentDescription* data() const {
            return attachments_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(attachments_.size());
        }

        ThinView add(
            const VkFormat format,
            const VkImageLayout final_layout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            const VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            const VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
            const VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE,
            const VkAttachmentLoadOp stencil_load_op =
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            const VkAttachmentStoreOp stencil_store_op =
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
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

            return ThinView{ added };
        }

    private:
        std::vector<VkAttachmentDescription> attachments_;
    };


    class AttachmentRefBuilder {

    public:
        const VkAttachmentReference* data() const {
            if (attachments_.empty())
                return nullptr;
            else
                return attachments_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(attachments_.size());
        }

        AttachmentRefBuilder& add(uint32_t index, VkImageLayout layout) {
            auto& added = attachments_.emplace_back();
            added = {};

            added.attachment = index;
            added.layout = layout;

            return *this;
        }

    private:
        std::vector<VkAttachmentReference> attachments_;
    };


    class SubpassDependencyBuilder {

    public:
        class View {

        public:
            View(VkSubpassDependency& dep) : dep_{ dep } {}

        private:
            VkSubpassDependency& dep_;
        };

        View& add() {
            auto& dependency = data_.emplace_back();
            dependency = {};

            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            return View{ dependency };
        }

        auto data() const { return data_.data(); }
        auto size() const { return static_cast<uint32_t>(data_.size()); }

    private:
        std::vector<VkSubpassDependency> data_;
    };


    class ShaderModule {

    public:
        ShaderModule(
            const mirinae::respath_t& spv_path, mirinae::VulkanDevice& device
        )
            : device_{ device } {
            if (auto spv = device.filesys().read_file_to_vector(spv_path)) {
                if (auto shader = this->create_shader_module(
                        *spv, device.logi_device()
                    )) {
                    handle_ = shader.value();
                } else {
                    throw std::runtime_error{ fmt::format(
                        "Failed to create shader module with given data: {}",
                        spv_path.u8string()
                    ) };
                }
            } else {
                throw std::runtime_error{ fmt::format(
                    "Failed to read a shader file: {}", spv_path.u8string()
                ) };
            }
        }

        ~ShaderModule() {
            if (VK_NULL_HANDLE != handle_) {
                vkDestroyShaderModule(device_.logi_device(), handle_, nullptr);
                handle_ = VK_NULL_HANDLE;
            }
        }

        VkShaderModule get() const { return handle_; }

    private:
        static std::optional<VkShaderModule> create_shader_module(
            const std::vector<uint8_t>& spv, VkDevice logi_device
        ) {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = spv.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(
                    logi_device, &createInfo, nullptr, &shaderModule
                ) != VK_SUCCESS) {
                return std::nullopt;
            }

            return shaderModule;
        }

        mirinae::VulkanDevice& device_;
        VkShaderModule handle_ = VK_NULL_HANDLE;
    };

}  // namespace


// Pipeline builders
namespace {

    class ColorBlendAttachmentStateBuilder {

    public:
        const VkPipelineColorBlendAttachmentState* data() const {
            return data_.data();
        }

        uint32_t size() const { return static_cast<uint32_t>(data_.size()); }

        template <bool TBlendingEnabled>
        void add() {
            auto& added = data_.emplace_back();
            added.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                   VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT |
                                   VK_COLOR_COMPONENT_A_BIT;
            added.colorBlendOp = VK_BLEND_OP_ADD;
            added.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            added.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            added.alphaBlendOp = VK_BLEND_OP_ADD;

            if constexpr (TBlendingEnabled) {
                added.blendEnable = VK_TRUE;
                added.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                added.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            } else {
                added.blendEnable = VK_FALSE;
                added.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                added.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            }
        }

    private:
        std::vector<VkPipelineColorBlendAttachmentState> data_;
    };


    auto create_info_shader_stages_pair(
        const ShaderModule& vertex, const ShaderModule& fragment
    ) {
        std::array<VkPipelineShaderStageCreateInfo, 2> output{};
        {
            auto& shader_info = output[0];
            shader_info.sType =
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_info.module = vertex.get();
            shader_info.pName = "main";
        }
        {
            auto& shader_info = output[1];
            shader_info.sType =
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_info.module = fragment.get();
            shader_info.pName = "main";
        }
        return output;
    }

    auto create_info_dynamic_states(const VkDynamicState* array, size_t size) {
        VkPipelineDynamicStateCreateInfo output{};
        output.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        output.dynamicStateCount = static_cast<uint32_t>(size);
        output.pDynamicStates = array;
        return output;
    }

    auto create_info_vertex_input_states(
        const VkVertexInputBindingDescription* binding_descriptions = nullptr,
        size_t binding_count = 0,
        const VkVertexInputAttributeDescription* attrib_descriptions = nullptr,
        size_t attrib_count = 0
    ) {
        VkPipelineVertexInputStateCreateInfo output{};
        output.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        output.vertexBindingDescriptionCount = binding_count;
        output.pVertexBindingDescriptions = binding_descriptions;
        output.vertexAttributeDescriptionCount = attrib_count;
        output.pVertexAttributeDescriptions = attrib_descriptions;
        return output;
    }

    auto create_info_input_assembly() {
        VkPipelineInputAssemblyStateCreateInfo output{};
        output.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        output.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        output.primitiveRestartEnable = VK_FALSE;
        return output;
    }

    auto create_info_viewport_state() {
        VkPipelineViewportStateCreateInfo output{};
        output.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        output.viewportCount = 1;
        output.pViewports = nullptr;
        output.scissorCount = 1;
        output.pScissors = nullptr;
        return output;
    }

    auto create_info_rasterizer(
        const VkCullModeFlags cull_mode,
        const bool enable_bias,
        const float bias_constant,
        const float bias_slope,
        const bool enable_depth_clamp = false
    ) {
        VkPipelineRasterizationStateCreateInfo output{};
        output.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

        // vulkan-tutorial.com said this requires GPU feature enabled.
        output.depthClampEnable = VK_FALSE;

        // Discards all fragents. But why would you ever want it? Well, check
        // the link below. https://stackoverflow.com/questions/42470669/
        // when-does-it-make-sense-to-turn-off-the-rasterization-step
        output.rasterizerDiscardEnable = VK_FALSE;

        // Any mode other than FILL requires GPU feature enabled.
        output.polygonMode = VK_POLYGON_MODE_FILL;

        // GPU feature, `wideLines` required for lines thicker than 1.
        output.lineWidth = 1;

        output.cullMode = cull_mode;
        output.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        output.depthBiasEnable = enable_bias ? VK_TRUE : VK_FALSE;
        output.depthBiasConstantFactor = bias_constant;
        output.depthBiasSlopeFactor = bias_slope;
        output.depthBiasClamp = 0;
        output.depthClampEnable = enable_depth_clamp ? VK_TRUE : VK_FALSE;

        return output;
    }

    auto create_info_multisampling() {
        VkPipelineMultisampleStateCreateInfo output{};
        output.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        output.sampleShadingEnable = VK_FALSE;
        output.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        output.minSampleShading = 1;
        output.pSampleMask = nullptr;
        output.alphaToCoverageEnable = VK_FALSE;
        output.alphaToOneEnable = VK_FALSE;
        return output;
    }

    auto create_info_depth_stencil(bool depth_test, bool depth_write) {
        VkPipelineDepthStencilStateCreateInfo output{};
        output.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        output.depthTestEnable = depth_test ? VK_TRUE : VK_FALSE;
        output.depthWriteEnable = depth_write ? VK_TRUE : VK_FALSE;
        output.depthCompareOp = VK_COMPARE_OP_LESS;
        output.depthBoundsTestEnable = VK_FALSE;
        output.minDepthBounds = 0;
        output.maxDepthBounds = 1;
        output.stencilTestEnable = VK_FALSE;
        output.front = {};
        output.back = {};
        return output;
    }

    auto create_info_color_blend(
        const ::ColorBlendAttachmentStateBuilder& color_blend_attachments
    ) {
        VkPipelineColorBlendStateCreateInfo output{};
        output.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        output.logicOpEnable = VK_FALSE;
        output.logicOp = VK_LOGIC_OP_COPY;
        output.attachmentCount = color_blend_attachments.size();
        output.pAttachments = color_blend_attachments.data();
        output.blendConstants[0] = 0;
        output.blendConstants[1] = 0;
        output.blendConstants[2] = 0;
        output.blendConstants[3] = 0;
        return output;
    }

}  // namespace


// gbuf
namespace { namespace gbuf {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        DescLayoutBuilder builder{ "gbuf:model" };
        builder
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_GbufModel
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // Albedo map
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // Normal map
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        DescLayoutBuilder builder{ "gbuf:actor" };
        builder.add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1);  // U_GbufActor
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat depth,
        VkFormat albedo,
        VkFormat normal,
        VkFormat material,
        VkDevice logi_device
    ) {
        ::AttachmentDescBuilder attach;
        attach.add(depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        attach.add(albedo, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        attach.add(normal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        attach.add(material, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        ::AttachmentRefBuilder color_attachment_refs;
        // albedo
        color_attachment_refs
            .add(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)   // albedo
            .add(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)   // normal
            .add(3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // material

        const VkAttachmentReference depth_attachment_ref{
            0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attach.size();
        create_info.pAttachments = attach.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS !=
            vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
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
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(
                device.logi_device(),
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS) {
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
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        auto binding_desc = ::make_vertex_static_binding_description();
        auto attrib_desc = ::make_vertex_static_attribute_descriptions();
        const auto vertex_input_info = ::create_info_vertex_input_states(
            &binding_desc, 1, attrib_desc.data(), attrib_desc.size()
        );

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<false>();
        color_blend_attachment_states.add<false>();
        color_blend_attachment_states.add<false>();
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        if (vkCreateGraphicsPipelines(
                device.logi_device(),
                VK_NULL_HANDLE,
                1,
                &pipeline_info,
                nullptr,
                &graphics_pipeline
            ) != VK_SUCCESS) {
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
            : device_(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
                fbuf_bundle.albedo().format(),
                fbuf_bundle.normal().format(),
                fbuf_bundle.material().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(3).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                formats_.at(1),
                formats_.at(2),
                formats_.at(3),
                device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_model(desclayouts, device),
                create_desclayout_actor(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);

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
                        fbuf_bundle.material().image_view(),
                    }
                ));
            }
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

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
        std::array<VkFormat, 4> formats_;
        std::array<VkClearValue, 4> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::gbuf


// gbuf skin
namespace { namespace gbuf_skin {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model");
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        DescLayoutBuilder builder{ "gbuf:actor_skinned" };
        builder.add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1);  // U_GbufActorSkinned
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat depth,
        VkFormat albedo,
        VkFormat normal,
        VkFormat material,
        VkDevice logi_device
    ) {
        ::AttachmentDescBuilder attach;
        attach.add(depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .initial_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE);
        attach.add(albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .initial_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE);
        attach.add(normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .initial_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE);
        attach.add(material, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .initial_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE);

        ::AttachmentRefBuilder color_attachment_refs;
        // albedo
        color_attachment_refs
            .add(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)   // albedo
            .add(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)   // normal
            .add(3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);  // material

        const VkAttachmentReference depth_attachment_ref{
            0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        ::SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attach.size();
        create_info.pAttachments = attach.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS !=
            vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
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
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        VkPipelineLayout pipelineLayout;
        const auto result = vkCreatePipelineLayout(
            device.logi_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout
        );
        if (VK_SUCCESS != result) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        return pipelineLayout;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderModule vert_shader{ "asset/spv/gbuf_skin_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/gbuf_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        auto binding_desc = ::make_vertex_skinned_binding_description();
        auto attrib_desc = ::make_vertex_skinned_attribute_descriptions();
        const auto vertex_input_info = ::create_info_vertex_input_states(
            &binding_desc, 1, attrib_desc.data(), attrib_desc.size()
        );

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<false>();
        color_blend_attachment_states.add<false>();
        color_blend_attachment_states.add<false>();
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        const auto result = vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        );
        if (VK_SUCCESS != result) {
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
            : device_(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
                fbuf_bundle.albedo().format(),
                fbuf_bundle.normal().format(),
                fbuf_bundle.material().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(3).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                formats_.at(1),
                formats_.at(2),
                formats_.at(3),
                device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_model(desclayouts, device),
                create_desclayout_actor(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);

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
                        fbuf_bundle.material().image_view(),
                    }
                ));
            }
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

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
        std::array<VkFormat, 4> formats_;
        std::array<VkClearValue, 4> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::gbuf_skin


// shadowmap
namespace { namespace shadowmap {

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor");
    }

    VkRenderPass create_renderpass(VkFormat depth, VkDevice logi_device) {
        ::AttachmentDescBuilder attach;
        attach.add(depth)
            .final_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE);

        ::AttachmentRefBuilder color_attach_refs;

        const VkAttachmentReference depth_attach_ref{
            0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attach_refs.size();
        subpass.pColorAttachments = color_attach_refs.data();
        subpass.pDepthStencilAttachment = &depth_attach_ref;

        SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attach.size();
        create_info.pAttachments = attach.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        const auto result = vkCreateRenderPass(
            logi_device, &create_info, nullptr, &output
        );
        if (VK_SUCCESS != result) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_actor, mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_actor,
        };

        VkPushConstantRange push_constant;
        {
            push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            push_constant.offset = 0;
            push_constant.size = sizeof(mirinae::U_ShadowPushConst);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &push_constant;
        }

        VkPipelineLayout pipelineLayout;
        const auto result = vkCreatePipelineLayout(
            device.logi_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout
        );
        if (VK_SUCCESS != result) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        return pipelineLayout;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderModule vert_shader{ "asset/spv/shadow_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/shadow_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        auto binding_desc = ::make_vertex_static_binding_description();
        auto attrib_desc = ::make_vertex_static_attribute_descriptions();
        const auto vertex_input_info = ::create_info_vertex_input_states(
            &binding_desc, 1, attrib_desc.data(), attrib_desc.size()
        );

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        const auto result = vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        );
        if (VK_SUCCESS != result) {
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
            : device_(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_actor(desclayouts, device), device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

        VkFramebuffer fbuf_at(uint32_t index) override {
            return VK_NULL_HANDLE;
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
    };

}}  // namespace ::shadowmap


// shadowmap skin
namespace { namespace shadowmap_skin {

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor_skinned");
    }

    VkRenderPass create_renderpass(VkFormat depth, VkDevice logi_device) {
        ::AttachmentDescBuilder attach;
        attach.add(depth)
            .initial_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .final_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE);

        ::AttachmentRefBuilder color_attach_refs;

        const VkAttachmentReference depth_attach_ref{
            0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attach_refs.size();
        subpass.pColorAttachments = color_attach_refs.data();
        subpass.pDepthStencilAttachment = &depth_attach_ref;

        SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attach.size();
        create_info.pAttachments = attach.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        const auto result = vkCreateRenderPass(
            logi_device, &create_info, nullptr, &output
        );
        if (VK_SUCCESS != result) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_actor, mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_actor,
        };

        VkPushConstantRange push_constant;
        {
            push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            push_constant.offset = 0;
            push_constant.size = sizeof(mirinae::U_ShadowPushConst);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &push_constant;
        }

        VkPipelineLayout pipelineLayout;
        const auto result = vkCreatePipelineLayout(
            device.logi_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout
        );
        if (VK_SUCCESS != result) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        return pipelineLayout;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderModule vert_shader{ "asset/spv/shadow_skin_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/shadow_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        auto binding_desc = ::make_vertex_skinned_binding_description();
        auto attrib_desc = ::make_vertex_skinned_attribute_descriptions();
        const auto vertex_input_info = ::create_info_vertex_input_states(
            &binding_desc, 1, attrib_desc.data(), attrib_desc.size()
        );

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        const auto result = vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        );
        if (VK_SUCCESS != result) {
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
            : device_(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_actor(desclayouts, device), device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

        VkFramebuffer fbuf_at(uint32_t index) override {
            return VK_NULL_HANDLE;
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
    };

}}  // namespace ::shadowmap_skin


// compo
namespace { namespace compo {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        DescLayoutBuilder builder{ "compo:main" };
        builder
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)    // depth
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)    // albedo
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)    // normal
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)    // material
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_CompoMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);   // dlight shadowmap
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat compo_format, VkDevice logi_device
    ) {
        ::AttachmentDescBuilder attachments;
        attachments.add(
            compo_format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(
            0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );  // compo

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = nullptr;

        ::SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS !=
            vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_main, mirinae::VulkanDevice& device
    ) {
        const std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_main,
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(
                device.logi_device(),
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        return pipelineLayout;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderModule vert_shader{ "asset/spv/compo_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/compo_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        const auto vertex_input_info = ::create_info_vertex_input_states();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(false, false);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<false>();
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        if (vkCreateGraphicsPipelines(
                device.logi_device(),
                VK_NULL_HANDLE,
                1,
                &pipeline_info,
                nullptr,
                &graphics_pipeline
            ) != VK_SUCCESS) {
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
            : device_(device) {
            formats_ = {
                fbuf_bundle.compo().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_main(desclayouts, device), device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    width,
                    height,
                    renderpass_,
                    device.logi_device(),
                    {
                        fbuf_bundle.compo().image_view(),
                    }
                ));
            }
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

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

}}  // namespace ::compo


// transp
namespace { namespace transp {

    VkDescriptorSetLayout create_desclayout_frame(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        DescLayoutBuilder builder{ "transp:frame" };
        builder.add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // U_CompoMain
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model");
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor");
    }

    VkRenderPass create_renderpass(
        VkFormat compo_format, VkFormat depth_format, VkDevice logi_device
    ) {
        ::AttachmentDescBuilder attachments;
        attachments.add(compo_format)
            .initial_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .final_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
        attachments.add(depth_format)
            .initial_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .final_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD);

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(
            0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );  // compo

        VkAttachmentReference depth_attachment_ref{
            1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        ::SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS !=
            vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_frame,
        VkDescriptorSetLayout desclayout_model,
        VkDescriptorSetLayout desclayout_actor,
        mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_frame,
            desclayout_model,
            desclayout_actor,
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(
                device.logi_device(),
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        return pipelineLayout;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderModule vert_shader{ "asset/spv/transp_vert.spv", device };
        ::ShaderModule frag_shader{ "asset/spv/transp_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        auto binding_desc = ::make_vertex_static_binding_description();
        auto attrib_desc = ::make_vertex_static_attribute_descriptions();
        const auto vertex_input_info = ::create_info_vertex_input_states(
            &binding_desc, 1, attrib_desc.data(), attrib_desc.size()
        );

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, false);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<true>();
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        if (vkCreateGraphicsPipelines(
                device.logi_device(),
                VK_NULL_HANDLE,
                1,
                &pipeline_info,
                nullptr,
                &graphics_pipeline
            ) != VK_SUCCESS) {
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
            : device_(device) {
            formats_ = {
                fbuf_bundle.compo().format(),
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_frame(desclayouts, device),
                create_desclayout_model(desclayouts, device),
                create_desclayout_actor(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    width,
                    height,
                    renderpass_,
                    device.logi_device(),
                    {
                        fbuf_bundle.compo().image_view(),
                        fbuf_bundle.depth().image_view(),
                    }
                ));
            }
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

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
        std::array<VkFormat, 2> formats_;
        std::array<VkClearValue, 2> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::transp


// transp skin
namespace { namespace transp_skin {

    VkDescriptorSetLayout create_desclayout_frame(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("transp:frame");
    }

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model");
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor_skinned");
    }

    VkRenderPass create_renderpass(
        VkFormat compo_format, VkFormat depth_format, VkDevice logi_device
    ) {
        ::AttachmentDescBuilder attach;
        attach.add(compo_format)
            .initial_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .final_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD);
        attach.add(depth_format)
            .initial_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .final_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD);

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(
            0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );  // compo

        VkAttachmentReference depth_attachment_ref{
            1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        ::SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attach.size();
        create_info.pAttachments = attach.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS !=
            vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_frame,
        VkDescriptorSetLayout desclayout_model,
        VkDescriptorSetLayout desclayout_actor,
        mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_frame,
            desclayout_model,
            desclayout_actor,
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(
                device.logi_device(),
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        return pipelineLayout;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderModule vert_shader{ "asset/spv/transp_skin_vert.spv",
                                    device };
        ::ShaderModule frag_shader{ "asset/spv/transp_frag.spv", device };
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        auto binding_desc = ::make_vertex_skinned_binding_description();
        auto attrib_desc = ::make_vertex_skinned_attribute_descriptions();
        const auto vertex_input_info = ::create_info_vertex_input_states(
            &binding_desc, 1, attrib_desc.data(), attrib_desc.size()
        );

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, false);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<true>();
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        const auto result = vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        );
        if (VK_SUCCESS != result) {
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
            : device_(device) {
            formats_ = {
                fbuf_bundle.compo().format(),
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_frame(desclayouts, device),
                create_desclayout_model(desclayouts, device),
                create_desclayout_actor(desclayouts, device),
                device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    width,
                    height,
                    renderpass_,
                    device.logi_device(),
                    {
                        fbuf_bundle.compo().image_view(),
                        fbuf_bundle.depth().image_view(),
                    }
                ));
            }
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

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
        std::array<VkFormat, 2> formats_;
        std::array<VkClearValue, 2> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::transp_skin


// fillscreen
namespace { namespace fillscreen {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        DescLayoutBuilder builder{ "fillscreen:main" };
        builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // compo
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

        ::SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS !=
            vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_main, mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_main,
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(
                device.logi_device(),
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS) {
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
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        const auto vertex_input_info = ::create_info_vertex_input_states();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(false, false);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<false>();
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        if (vkCreateGraphicsPipelines(
                device.logi_device(),
                VK_NULL_HANDLE,
                1,
                &pipeline_info,
                nullptr,
                &graphics_pipeline
            ) != VK_SUCCESS) {
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
            : device_(device) {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_main(desclayouts, device), device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    swapchain.width(),
                    swapchain.height(),
                    renderpass_,
                    device.logi_device(),
                    {
                        swapchain.view_at(i),
                    }
                ));
            }
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

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

}}  // namespace ::fillscreen


// overlay
namespace { namespace overlay {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        DescLayoutBuilder builder{ "overlay:main" };
        builder
            .add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1)    // U_OverlayMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // color
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // mask
        return builder.build_in_place(desclayouts, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat surface, VkDevice logi_device) {
        ::AttachmentDescBuilder attachments;
        attachments.add(
            surface,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ATTACHMENT_LOAD_OP_LOAD
        );

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = nullptr;

        ::SubpassDependencyBuilder dependency;
        dependency.add();

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = dependency.size();
        create_info.pDependencies = dependency.data();

        VkRenderPass output = VK_NULL_HANDLE;
        if (VK_SUCCESS !=
            vkCreateRenderPass(logi_device, &create_info, nullptr, &output)) {
            throw std::runtime_error("failed to create render pass!");
        }

        return output;
    }

    VkPipelineLayout create_pipeline_layout(
        VkDescriptorSetLayout desclayout_main, mirinae::VulkanDevice& device
    ) {
        std::vector<VkDescriptorSetLayout> desclayouts{
            desclayout_main,
        };

        VkPushConstantRange push_constant;
        {
            push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                       VK_SHADER_STAGE_FRAGMENT_BIT;
            push_constant.offset = 0;
            push_constant.size = sizeof(mirinae::U_OverlayPushConst);
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType =
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(
                desclayouts.size()
            );
            pipelineLayoutInfo.pSetLayouts = desclayouts.data();
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &push_constant;
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(
                device.logi_device(),
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS) {
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
        const auto shader_stages = ::create_info_shader_stages_pair(
            vert_shader, frag_shader
        );

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        const auto vertex_input_info = ::create_info_vertex_input_states();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(false, false);

        ColorBlendAttachmentStateBuilder color_blend_attachment_states;
        color_blend_attachment_states.add<true>();
        const auto color_blending = ::create_info_color_blend(
            color_blend_attachment_states
        );

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
        if (vkCreateGraphicsPipelines(
                device.logi_device(),
                VK_NULL_HANDLE,
                1,
                &pipeline_info,
                nullptr,
                &graphics_pipeline
            ) != VK_SUCCESS) {
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
            : device_(device) {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = create_pipeline_layout(
                create_desclayout_main(desclayouts, device), device
            );
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(::create_framebuffer(
                    swapchain.width(),
                    swapchain.height(),
                    renderpass_,
                    device.logi_device(),
                    {
                        swapchain.view_at(i),
                    }
                ));
            }
        }

        ~RenderPassBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkRenderPass renderpass() override { return renderpass_; }

        VkPipeline pipeline() override { return pipeline_; }

        VkPipelineLayout pipeline_layout() override { return layout_; }

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

}}  // namespace ::overlay


// RenderPassPackage
namespace mirinae {

    void RenderPassPackage::init(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        gbuf_ = std::make_unique<::gbuf::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        gbuf_skin_ = std::make_unique<::gbuf_skin::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        shadowmap_ = std::make_unique<::shadowmap::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        shadowmap_skin_ = std::make_unique<::shadowmap_skin::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        compo_ = std::make_unique<::compo::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        transp_ = std::make_unique<::transp::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        transp_skin_ =
            std::make_unique<::transp_skin::RenderPassBundle>(
                width, height, fbuf_bundle, desclayouts, swapchain, device
            );
        fillscreen_ = std::make_unique<::fillscreen::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        overlay_ = std::make_unique<::overlay::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
    }

    void RenderPassPackage::destroy() {
        gbuf_.reset();
        gbuf_skin_.reset();
        shadowmap_.reset();
        shadowmap_skin_.reset();
        compo_.reset();
        transp_.reset();
        transp_skin_.reset();
        fillscreen_.reset();
        overlay_.reset();
    }

}  // namespace mirinae
