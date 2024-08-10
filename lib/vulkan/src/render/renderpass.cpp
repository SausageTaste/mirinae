#include "mirinae/render/renderpass.hpp"

#include <stdexcept>

#include <spdlog/spdlog.h>

#include "mirinae/render/vkmajorplayers.hpp"


// Builders
namespace {

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
        VK_CHECK(
            vkCreateFramebuffer(logi_device, &framebufferInfo, nullptr, &output)
        );

        return output;
    }


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

        View add() {
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

            return View{ data_.back() };
        }

        auto data() const { return data_.data(); }
        auto size() const { return static_cast<uint32_t>(data_.size()); }

    private:
        std::vector<VkSubpassDependency> data_;
    };


    class PipelineLayoutBuilder {

    public:
        PipelineLayoutBuilder& reset_stage_flags(VkShaderStageFlags flags) {
            pc_stage_flags_ = 0;
            return *this;
        }

        PipelineLayoutBuilder& add_vertex_flag() {
            pc_stage_flags_ |= VK_SHADER_STAGE_VERTEX_BIT;
            return *this;
        }

        PipelineLayoutBuilder& add_frag_flag() {
            pc_stage_flags_ |= VK_SHADER_STAGE_FRAGMENT_BIT;
            return *this;
        }

        PipelineLayoutBuilder& pc(uint32_t offset, uint32_t size) {
            auto& added = pc_ranges_.emplace_back();
            added.stageFlags = pc_stage_flags_;
            added.offset = offset;
            added.size = size;
            return *this;
        }

        PipelineLayoutBuilder& desc(VkDescriptorSetLayout layout) {
            desclayouts_.push_back(layout);
            return *this;
        }

        VkPipelineLayout build(mirinae::VulkanDevice& device) {
            VkPipelineLayoutCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            create_info.setLayoutCount = (uint32_t)desclayouts_.size();
            create_info.pSetLayouts = desclayouts_.data();
            create_info.pushConstantRangeCount = (uint32_t)pc_ranges_.size();
            create_info.pPushConstantRanges = pc_ranges_.data();

            VkPipelineLayout output = VK_NULL_HANDLE;
            VK_CHECK(vkCreatePipelineLayout(
                device.logi_device(), &create_info, nullptr, &output
            ));
            return output;
        }

    private:
        std::vector<VkDescriptorSetLayout> desclayouts_;
        std::vector<VkPushConstantRange> pc_ranges_;
        VkShaderStageFlags pc_stage_flags_ = 0;
    };

}  // namespace


// Pipeline builders
namespace {

    class ShaderStagesBuilder {

    public:
        ShaderStagesBuilder(mirinae::VulkanDevice& device)
            : device_{ device } {}

        ~ShaderStagesBuilder() {
            for (auto& module : modules_) {
                vkDestroyShaderModule(device_.logi_device(), module, nullptr);
            }
            modules_.clear();
        }

        ShaderStagesBuilder& add_vert(const mirinae::respath_t& spv_path) {
            modules_.push_back(this->load_spv(spv_path, device_));
            this->add_stage(VK_SHADER_STAGE_VERTEX_BIT, modules_.back());
            return *this;
        }

        ShaderStagesBuilder& add_frag(const mirinae::respath_t& spv_path) {
            modules_.push_back(this->load_spv(spv_path, device_));
            this->add_stage(VK_SHADER_STAGE_FRAGMENT_BIT, modules_.back());
            return *this;
        }

        const VkPipelineShaderStageCreateInfo* data() const {
            return stages_.data();
        }
        uint32_t size() const { return static_cast<uint32_t>(stages_.size()); }

    private:
        void add_stage(VkShaderStageFlagBits stage, VkShaderModule module) {
            auto& added = stages_.emplace_back();

            added.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            added.stage = stage;
            added.module = module;
            added.pName = "main";
        }

        static VkShaderModule load_spv(
            const mirinae::respath_t& spv_path, mirinae::VulkanDevice& device
        ) {
            const auto spv = device.filesys().read_file(spv_path);
            if (!spv) {
                throw std::runtime_error{ fmt::format(
                    "Failed to read a shader file: {}", spv_path.u8string()
                ) };
            }

            const auto sha = create_shader_module(*spv, device.logi_device());
            if (!sha) {
                throw std::runtime_error{ fmt::format(
                    "Failed to create shader module with given data: {}",
                    spv_path.u8string()
                ) };
            }

            return sha.value();
        }

        static std::optional<VkShaderModule> create_shader_module(
            const std::vector<uint8_t>& spv, VkDevice logi_device
        ) {
            VkShaderModuleCreateInfo cinfo{};
            cinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            cinfo.codeSize = spv.size();
            cinfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());

            VkShaderModule output = VK_NULL_HANDLE;
            const auto result = vkCreateShaderModule(
                logi_device, &cinfo, nullptr, &output
            );

            if (result != VK_SUCCESS)
                return std::nullopt;
            else
                return output;
        }

        mirinae::VulkanDevice& device_;
        std::vector<VkPipelineShaderStageCreateInfo> stages_;
        std::vector<VkShaderModule> modules_;
    };


    class VertexInputStateBuilder {

    public:
        VkPipelineVertexInputStateCreateInfo build() const {
            VkPipelineVertexInputStateCreateInfo output{};
            output.sType =
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            output.vertexBindingDescriptionCount = (uint32_t)bindings_.size();
            output.pVertexBindingDescriptions = bindings_.data();
            output.vertexAttributeDescriptionCount = (uint32_t)attribs_.size();
            output.pVertexAttributeDescriptions = attribs_.data();
            return output;
        }

        VertexInputStateBuilder& set_static() {
            this->set_binding_static();
            this->set_attrib_static();
            return *this;
        }

        VertexInputStateBuilder& set_skinned() {
            this->set_binding_skinned();
            this->set_attrib_skinned();
            return *this;
        }

    private:
        void add_attrib(VkFormat format, uint32_t offset) {
            const auto location = static_cast<uint32_t>(attribs_.size());

            auto& one = attribs_.emplace_back();
            one.binding = 0;
            one.location = location;
            one.format = format;
            one.offset = offset;
        }

        void add_attrib_vec2(uint32_t offset) {
            this->add_attrib(VK_FORMAT_R32G32_SFLOAT, offset);
        }

        void add_attrib_vec3(uint32_t offset) {
            this->add_attrib(VK_FORMAT_R32G32B32_SFLOAT, offset);
        }

        void add_attrib_vec4(uint32_t offset) {
            this->add_attrib(VK_FORMAT_R32G32B32A32_SFLOAT, offset);
        }

        void add_attrib_ivec4(uint32_t offset) {
            this->add_attrib(VK_FORMAT_R32G32B32A32_SINT, offset);
        }

        void set_binding_static() {
            bindings_.clear();
            auto& one = bindings_.emplace_back();

            one.binding = 0;
            one.stride = sizeof(mirinae::VertexStatic);
            one.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        }

        void set_binding_skinned() {
            bindings_.clear();
            auto& one = bindings_.emplace_back();

            one.binding = 0;
            one.stride = sizeof(mirinae::VertexSkinned);
            one.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        }

        void set_attrib_static() {
            attribs_.clear();

            this->add_attrib_vec3(offsetof(mirinae::VertexStatic, pos_));
            this->add_attrib_vec3(offsetof(mirinae::VertexStatic, normal_));
            this->add_attrib_vec3(offsetof(mirinae::VertexStatic, tangent_));
            this->add_attrib_vec2(offsetof(mirinae::VertexStatic, texcoord_));
        }

        void set_attrib_skinned() {
            using Vertex = mirinae::VertexSkinned;
            attribs_.clear();

            this->add_attrib_vec3(offsetof(Vertex, pos_));
            this->add_attrib_vec3(offsetof(Vertex, normal_));
            this->add_attrib_vec3(offsetof(Vertex, tangent_));
            this->add_attrib_vec2(offsetof(Vertex, uv_));
            this->add_attrib_vec4(offsetof(Vertex, joint_weights_));
            this->add_attrib_ivec4(offsetof(Vertex, joint_indices_));
        }

        std::vector<VkVertexInputBindingDescription> bindings_;
        std::vector<VkVertexInputAttributeDescription> attribs_;
    };


    class ColorBlendStateBuilder {

    public:
        ColorBlendStateBuilder& add(bool blend_enabled) {
            auto& added = data_.emplace_back();
            added.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                   VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT |
                                   VK_COLOR_COMPONENT_A_BIT;
            added.colorBlendOp = VK_BLEND_OP_ADD;
            added.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            added.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            added.alphaBlendOp = VK_BLEND_OP_ADD;

            if (blend_enabled) {
                added.blendEnable = VK_TRUE;
                added.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                added.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            } else {
                added.blendEnable = VK_FALSE;
                added.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                added.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            }

            return *this;
        }

        ColorBlendStateBuilder& add(bool blend_enabled, size_t count) {
            if (count < 1)
                return *this;

            this->add(blend_enabled);
            for (size_t i = 1; i < count; ++i) data_.push_back(data_.back());
            return *this;
        }

        VkPipelineColorBlendStateCreateInfo build() const {
            VkPipelineColorBlendStateCreateInfo output{};
            output.sType =
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            output.logicOpEnable = VK_FALSE;
            output.logicOp = VK_LOGIC_OP_COPY;
            output.attachmentCount = static_cast<uint32_t>(data_.size());
            output.pAttachments = data_.data();
            output.blendConstants[0] = 0;
            output.blendConstants[1] = 0;
            output.blendConstants[2] = 0;
            output.blendConstants[3] = 0;
            return output;
        }

    private:
        std::vector<VkPipelineColorBlendAttachmentState> data_;
    };


    auto create_info_dynamic_states(const VkDynamicState* array, size_t size) {
        VkPipelineDynamicStateCreateInfo output{};
        output.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        output.dynamicStateCount = static_cast<uint32_t>(size);
        output.pDynamicStates = array;
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

}  // namespace


// gbuf
namespace { namespace gbuf {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf:model" };
        builder
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_GbufModel
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // Albedo map
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // Normal map
        return desclayouts.add(builder, device.logi_device());
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf:actor" };
        builder.add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1);  // U_GbufActor
        return desclayouts.add(builder, device.logi_device());
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/gbuf_vert.spv");
        shader_stages.add_frag(":asset/spv/gbuf_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.set_static().build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(false, 3).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
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
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .build(device);
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
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf:actor_skinned" };
        builder.add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1);  // U_GbufActorSkinned
        return desclayouts.add(builder, device.logi_device());
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/gbuf_skin_vert.spv");
        shader_stages.add_frag(":asset/spv/gbuf_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.set_skinned().build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(false, 3).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
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
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .build(device);
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
        return desclayouts.get("gbuf:actor").layout();
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/shadow_vert.spv");
        shader_stages.add_frag(":asset/spv/shadow_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.set_static().build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, true, 80, 8, device.has_supp_depth_clamp()
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_actor(desclayouts, device))
                          .add_vertex_flag()
                          .pc(0, sizeof(mirinae::U_ShadowPushConst))
                          .build(device);
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
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::shadowmap


// shadowmap skin
namespace { namespace shadowmap_skin {

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor_skinned").layout();
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/shadow_skin_vert.spv");
        shader_stages.add_frag(":asset/spv/shadow_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.set_skinned().build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, true, 80, 8, device.has_supp_depth_clamp()
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_actor(desclayouts, device))
                          .add_vertex_flag()
                          .pc(0, sizeof(mirinae::U_ShadowPushConst))
                          .build(device);
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
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::shadowmap_skin


// cubemap
namespace { namespace cubemap {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor").layout();
    }

    VkRenderPass create_renderpass(
        VkFormat depth, VkFormat color, VkDevice logi_device
    ) {
        ::AttachmentDescBuilder attach;
        attach.add(depth)
            .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE)
            .initial_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .final_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        attach.add(color)
            .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE)
            .initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .final_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        ::AttachmentRefBuilder color_attachment_refs;
        // albedo
        color_attachment_refs.add(
            1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );  // color

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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/envmap_vert.spv");
        shader_stages.add_frag(":asset/spv/envmap_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.set_static().build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, true);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(false, 1).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

        return graphics_pipeline;
    }


    class RenderPassBundle : public mirinae::IRenderPassBundle {

    public:
        RenderPassBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = { mirinae::select_depth_map_format(device),
                         VK_FORMAT_B10G11R11_UFLOAT_PACK32 };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .add_vertex_flag()
                          .pc(0, sizeof(mirinae::U_EnvmapPushConst))
                          .build(device);
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
        std::array<VkFormat, 2> formats_;
        std::array<VkClearValue, 2> clear_values_;
    };

}}  // namespace ::cubemap


// envdiffuse
namespace { namespace envdiffuse {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "envdiffuse:main" };
        builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // envmap
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat color, VkDevice logi_device) {
        ::AttachmentDescBuilder attach;
        attach.add(color)
            .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
            .store_op(VK_ATTACHMENT_STORE_OP_STORE)
            .initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .final_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        ::AttachmentRefBuilder color_attach_refs;
        color_attach_refs.add(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attach_refs.size();
        subpass.pColorAttachments = color_attach_refs.data();
        subpass.pDepthStencilAttachment = nullptr;

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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/envdiffuse_vert.spv");
        shader_stages.add_frag(":asset/spv/envdiffuse_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(false, false);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(false, 1).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

        return graphics_pipeline;
    }


    class RenderPassBundle : public mirinae::IRenderPassBundle {

    public:
        RenderPassBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = { VK_FORMAT_B10G11R11_UFLOAT_PACK32 };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_vertex_flag()
                          .pc(0, sizeof(mirinae::U_EnvdiffusePushConst))
                          .build(device);
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
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::envdiffuse


// compo
namespace { namespace compo {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "compo:main" };
        builder
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // depth
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // albedo
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // normal
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // material
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_CompoMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // dlight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // slight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // envmap
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat compo_format, VkDevice logi_device
    ) {
        ::AttachmentDescBuilder attachments;
        attachments.add(compo_format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/compo_vert.spv");
        shader_stages.add_frag(":asset/spv/compo_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(false, false);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(false).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.compo().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .build(device);
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
        mirinae::DescLayoutBuilder builder{ "transp:frame" };
        builder
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_CompoMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // dlight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // slight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // envmap
        return desclayouts.add(builder, device.logi_device());
    }

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor").layout();
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/transp_vert.spv");
        shader_stages.add_frag(":asset/spv/transp_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.set_static().build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, false);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(true).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.compo().format(),
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_frame(desclayouts, device))
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .build(device);
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
        return desclayouts.get("transp:frame").layout();
    }

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor_skinned").layout();
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/transp_skin_vert.spv");
        shader_stages.add_frag(":asset/spv/transp_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.set_skinned().build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_NONE, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(true, false);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(true).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.compo().format(),
                fbuf_bundle.depth().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_frame(desclayouts, device))
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .build(device);
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
        mirinae::DescLayoutBuilder builder{ "fillscreen:main" };
        builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // compo
        return desclayouts.add(builder, device.logi_device());
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/fill_screen_vert.spv");
        shader_stages.add_frag(":asset/spv/fill_screen_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(false, false);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(false).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .build(device);
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
        mirinae::DescLayoutBuilder builder{ "overlay:main" };
        builder
            .add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1)    // U_OverlayMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // color
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // mask
        return desclayouts.add(builder, device.logi_device());
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
        VK_CHECK(vkCreateRenderPass(logi_device, &create_info, NULL, &output));

        return output;
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        ::ShaderStagesBuilder shader_stages{ device };
        shader_stages.add_vert(":asset/spv/overlay_vert.spv");
        shader_stages.add_frag(":asset/spv/overlay_frag.spv");

        std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const auto dynamic_state_info = ::create_info_dynamic_states(
            dynamic_states.data(), dynamic_states.size()
        );

        ::VertexInputStateBuilder vinput_builder;
        const auto vertex_input_info = vinput_builder.build();

        const auto input_assembly = ::create_info_input_assembly();

        const auto viewport_state = ::create_info_viewport_state();

        const auto rasterizer = ::create_info_rasterizer(
            VK_CULL_MODE_BACK_BIT, false, 0, 0, false
        );

        const auto multisampling = ::create_info_multisampling();

        const auto depth_stencil = ::create_info_depth_stencil(false, false);

        ::ColorBlendStateBuilder color_blend_builder;
        const auto color_blending = color_blend_builder.add(true).build();

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
        VK_CHECK(vkCreateGraphicsPipelines(
            device.logi_device(),
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &graphics_pipeline
        ));

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
            : IRenderPassBundle(device) {
            formats_ = {
                swapchain.format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_vertex_flag()
                          .add_frag_flag()
                          .pc(0, sizeof(mirinae::U_OverlayPushConst))
                          .build(device);
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
        cubemap_ = std::make_unique<::cubemap::RenderPassBundle>(
            desclayouts, device
        );
        envdiffuse_ = std::make_unique<::envdiffuse::RenderPassBundle>(
            desclayouts, device
        );
        compo_ = std::make_unique<::compo::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        transp_ = std::make_unique<::transp::RenderPassBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        transp_skin_ = std::make_unique<::transp_skin::RenderPassBundle>(
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
        cubemap_.reset();
        compo_.reset();
        transp_.reset();
        transp_skin_.reset();
        fillscreen_.reset();
        overlay_.reset();
    }

}  // namespace mirinae
