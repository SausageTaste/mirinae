#include "mirinae/vulkan_pch.h"

#include "mirinae/render/renderpass.hpp"

#include <stdexcept>

#include "mirinae/render/vkmajorplayers.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/common.hpp"


// Builders
namespace {

    VkFramebuffer create_framebuffer(
        uint32_t width,
        uint32_t height,
        VkRenderPass renderpass,
        VkDevice logi_device,
        const std::vector<VkImageView>& attachments
    ) {
        mirinae::FbufCinfo fbuf_cinfo;
        fbuf_cinfo.set_rp(renderpass)
            .add_attach(attachments)
            .set_dim(width, height);

        VkFramebuffer output = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFramebuffer(
            logi_device, &fbuf_cinfo.get(), nullptr, &output
        ));

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

        ShaderStagesBuilder& add_vert(const dal::path& spv_path) {
            modules_.push_back(this->load_spv(spv_path, device_));
            this->add_stage(VK_SHADER_STAGE_VERTEX_BIT, modules_.back());
            return *this;
        }

        ShaderStagesBuilder& add_frag(const dal::path& spv_path) {
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
            const dal::path& spv_path, mirinae::VulkanDevice& device
        ) {
            const auto spv = device.filesys().read_file(spv_path);
            if (!spv) {
                MIRINAE_ABORT(
                    "Failed to read a shader file: {}", spv_path.u8string()
                );
            }

            const auto sha = create_shader_module(*spv, device.logi_device());
            if (!sha) {
                MIRINAE_ABORT(
                    "Failed to create shader module with given data: {}",
                    spv_path.u8string()
                );
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
        output.depthCompareOp = VK_COMPARE_OP_GREATER;
        output.depthBoundsTestEnable = VK_FALSE;
        output.minDepthBounds = 0;
        output.maxDepthBounds = 1;
        output.stencilTestEnable = VK_FALSE;
        output.front = {};
        output.back = {};
        return output;
    }

}  // namespace


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

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(
                    ::create_framebuffer(
                        swapchain.width(),
                        swapchain.height(),
                        renderpass_,
                        device.logi_device(),
                        {
                            swapchain.view_at(i),
                        }
                    )
                );
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
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
            layout_ = ::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_vertex_flag()
                          .add_frag_flag()
                          .pc(0, sizeof(mirinae::U_OverlayPushConst))
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            for (int i = 0; i < swapchain.views_count(); ++i) {
                fbufs_.push_back(
                    ::create_framebuffer(
                        swapchain.width(),
                        swapchain.height(),
                        renderpass_,
                        device.logi_device(),
                        {
                            swapchain.view_at(i),
                        }
                    )
                );
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
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
