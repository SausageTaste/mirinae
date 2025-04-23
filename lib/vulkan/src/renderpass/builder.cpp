#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/builder.hpp"

#include "mirinae/render/meshdata.hpp"


// AttachDescView
namespace mirinae {

#define CLS RenderPassBuilder::AttachDescView

    CLS::AttachDescView(VkAttachmentDescription& desc) : desc_{ desc } {}

    CLS::~AttachDescView() = default;

    CLS& CLS::format(VkFormat x) {
        desc_.format = x;
        return *this;
    }

    CLS& CLS::samples(VkSampleCountFlagBits x) {
        desc_.samples = x;
        return *this;
    }

    CLS& CLS::load_op(VkAttachmentLoadOp x) {
        desc_.loadOp = x;
        return *this;
    }

    CLS& CLS::stor_op(VkAttachmentStoreOp x) {
        desc_.storeOp = x;
        return *this;
    }

    CLS& CLS::op_pair_clear_store() {
        desc_.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        desc_.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        return *this;
    }

    CLS& CLS::op_pair_load_store() {
        desc_.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        desc_.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        return *this;
    }

    CLS& CLS::stencil_load(VkAttachmentLoadOp x) {
        desc_.stencilLoadOp = x;
        return *this;
    }

    CLS& CLS::stencil_stor(VkAttachmentStoreOp x) {
        desc_.stencilStoreOp = x;
        return *this;
    }

    CLS& CLS::ini_layout(VkImageLayout x) {
        desc_.initialLayout = x;
        return *this;
    }

    CLS& CLS::fin_layout(VkImageLayout x) {
        desc_.finalLayout = x;
        return *this;
    }

    CLS& CLS::preset_default() {
        desc_ = {};
        // desc_.format = VK_FORMAT_UNDEFINED;
        desc_.samples = VK_SAMPLE_COUNT_1_BIT;
        desc_.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc_.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc_.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc_.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // desc_.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // desc_.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        return *this;
    }

#undef CLS

}  // namespace mirinae


// AttachDescBuilder
namespace mirinae {

#define CLS RenderPassBuilder::AttachDescBuilder

    const VkAttachmentDescription* CLS::data() const {
        if (data_.empty())
            return nullptr;
        else
            return data_.data();
    }

    uint32_t CLS::size() const { return static_cast<uint32_t>(data_.size()); }

    RenderPassBuilder::AttachDescView CLS::add(const VkFormat format) {
        auto& added = data_.emplace_back();
        AttachDescView view{ added };
        view.preset_default();
        view.format(format);
        return view;
    }

    RenderPassBuilder::AttachDescView CLS::dup(const VkFormat format) {
        const auto& last = data_.back();
        auto& added = data_.emplace_back(last);
        AttachDescView view{ added };
        view.format(format);
        return view;
    }

#undef CLS

}  // namespace mirinae


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
            MIRINAE_ABORT("failed to create render pass.");
        }

        return output;
    }

}  // namespace mirinae


// ShaderStagesBuilder
namespace mirinae {

#define CLS PipelineBuilder::ShaderStagesBuilder

    CLS::ShaderStagesBuilder(mirinae::VulkanDevice& device)
        : device_{ device } {}

    CLS::~ShaderStagesBuilder() {
        for (auto& module : modules_) {
            vkDestroyShaderModule(device_.logi_device(), module, nullptr);
        }
        modules_.clear();
    }

    CLS& CLS::add(
        const dal::path& spv_path, const VkShaderStageFlagBits stage
    ) {
        modules_.push_back(this->load_spv(spv_path, device_));
        this->add_stage(stage, modules_.back());
        return *this;
    }

    CLS& CLS::add_vert(const dal::path& spv_path) {
        return this->add(spv_path, VK_SHADER_STAGE_VERTEX_BIT);
    }

    CLS& CLS::add_frag(const dal::path& spv_path) {
        return this->add(spv_path, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    CLS& CLS::add_tesc(const dal::path& spv_path) {
        return this->add(spv_path, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    }

    CLS& CLS::add_tese(const dal::path& spv_path) {
        return this->add(spv_path, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }

    CLS& CLS::add_comp(const dal::path& spv_path) {
        return this->add(spv_path, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    const VkPipelineShaderStageCreateInfo* CLS::data() const {
        return stages_.data();
    }

    uint32_t CLS::size() const { return static_cast<uint32_t>(stages_.size()); }

    void CLS::add_stage(VkShaderStageFlagBits stage, VkShaderModule module) {
        auto& added = stages_.emplace_back();

        added.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        added.stage = stage;
        added.module = module;
        added.pName = "main";
    }

    VkShaderModule CLS::load_spv(
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

    std::optional<VkShaderModule> CLS::create_shader_module(
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

#undef CLS

}  // namespace mirinae


// VertexInputStateBuilder
namespace mirinae {

#define CLS PipelineBuilder::VertexInputStateBuilder

    CLS& CLS::set_static() {
        this->set_binding_static();
        this->set_attrib_static();
        return *this;
    }

    CLS& CLS::set_skinned() {
        this->set_binding_skinned();
        this->set_attrib_skinned();
        return *this;
    }

    VkPipelineVertexInputStateCreateInfo CLS::build() const {
        VkPipelineVertexInputStateCreateInfo out = {};
        out.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        out.vertexBindingDescriptionCount = static_cast<uint32_t>(
            bindings_.size()
        );
        out.pVertexBindingDescriptions = bindings_.data();
        out.vertexAttributeDescriptionCount = static_cast<uint32_t>(
            attribs_.size()
        );
        out.pVertexAttributeDescriptions = attribs_.data();
        return out;
    }

    void CLS::add_attrib(VkFormat format, uint32_t offset) {
        const auto location = static_cast<uint32_t>(attribs_.size());

        auto& one = attribs_.emplace_back();
        one.binding = 0;
        one.location = location;
        one.format = format;
        one.offset = offset;
    }

    void CLS::add_attrib_vec2(uint32_t offset) {
        this->add_attrib(VK_FORMAT_R32G32_SFLOAT, offset);
    }

    void CLS::add_attrib_vec3(uint32_t offset) {
        this->add_attrib(VK_FORMAT_R32G32B32_SFLOAT, offset);
    }

    void CLS::add_attrib_vec4(uint32_t offset) {
        this->add_attrib(VK_FORMAT_R32G32B32A32_SFLOAT, offset);
    }

    void CLS::add_attrib_ivec4(uint32_t offset) {
        this->add_attrib(VK_FORMAT_R32G32B32A32_SINT, offset);
    }

    void CLS::set_binding_static() {
        bindings_.clear();
        auto& one = bindings_.emplace_back();

        one.binding = 0;
        one.stride = sizeof(mirinae::VertexStatic);
        one.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }

    void CLS::set_binding_skinned() {
        bindings_.clear();
        auto& one = bindings_.emplace_back();

        one.binding = 0;
        one.stride = sizeof(mirinae::VertexSkinned);
        one.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }

    void CLS::set_attrib_static() {
        attribs_.clear();

        this->add_attrib_vec3(offsetof(mirinae::VertexStatic, pos_));
        this->add_attrib_vec3(offsetof(mirinae::VertexStatic, normal_));
        this->add_attrib_vec3(offsetof(mirinae::VertexStatic, tangent_));
        this->add_attrib_vec2(offsetof(mirinae::VertexStatic, texcoord_));
    }

    void CLS::set_attrib_skinned() {
        using Vertex = mirinae::VertexSkinned;
        attribs_.clear();

        this->add_attrib_vec3(offsetof(Vertex, pos_));
        this->add_attrib_vec3(offsetof(Vertex, normal_));
        this->add_attrib_vec3(offsetof(Vertex, tangent_));
        this->add_attrib_vec2(offsetof(Vertex, uv_));
        this->add_attrib_vec4(offsetof(Vertex, joint_weights_));
        this->add_attrib_ivec4(offsetof(Vertex, joint_indices_));
    }

#undef CLS

}  // namespace mirinae


// InputAssemblyStateBuilder
namespace mirinae {

#define CLS PipelineBuilder::InputAssemblyStateBuilder

    CLS::InputAssemblyStateBuilder() {
        info_ = {};
        info_.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        info_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        info_.primitiveRestartEnable = VK_FALSE;
    }

    CLS& CLS::topology(VkPrimitiveTopology top) {
        info_.topology = top;
        return *this;
    }

    CLS& CLS::topology_patch_list() {
        return this->topology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    }

#undef CLS

}  // namespace mirinae


// TessellationStateBuilder
namespace mirinae {

#define CLS PipelineBuilder::TessellationStateBuilder

    CLS::TessellationStateBuilder() {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    }

    CLS& CLS::patch_ctrl_points(uint32_t count) {
        info_.patchControlPoints = count;
        enabled_ = true;
        return *this;
    }

    const VkPipelineTessellationStateCreateInfo* CLS::get() const {
        return enabled_ ? &info_ : nullptr;
    }

#undef CLS

}  // namespace mirinae


// RasterizationStateBuilder
namespace mirinae {

#define CLS PipelineBuilder::RasterizationStateBuilder

    CLS::RasterizationStateBuilder() {
        info_ = {};
        info_.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

        // vulkan-tutorial.com said this requires GPU feature enabled.
        info_.depthClampEnable = VK_FALSE;

        // Discards all fragents. But why would you ever want it? Well,
        // check the link below.
        // https://stackoverflow.com/questions/42470669/when-does-it-make-sense-to-turn-off-the-rasterization-step
        info_.rasterizerDiscardEnable = VK_FALSE;

        // Any mode other than FILL requires GPU feature enabled.
        info_.polygonMode = VK_POLYGON_MODE_FILL;

        // GPU feature, `wideLines` required for lines thicker than 1.
        info_.lineWidth = 1;

        info_.cullMode = VK_CULL_MODE_NONE;
        info_.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        info_.depthBiasEnable = VK_FALSE;
        info_.depthBiasConstantFactor = 0;
        info_.depthBiasSlopeFactor = 0;
        info_.depthBiasClamp = 0;
        info_.depthClampEnable = VK_FALSE;
    }

    CLS& CLS::polygon_mode(VkPolygonMode mode) {
        info_.polygonMode = mode;
        return *this;
    }

    CLS& CLS::polygon_mode_line() {
        info_.polygonMode = VK_POLYGON_MODE_LINE;
        return *this;
    }

    CLS& CLS::line_width(float x) {
        info_.lineWidth = x;
        return *this;
    }

    CLS& CLS::cull_mode(VkCullModeFlags mode) {
        info_.cullMode = mode;
        return *this;
    }

    CLS& CLS::cull_mode_back() {
        info_.cullMode = VK_CULL_MODE_BACK_BIT;
        return *this;
    }

    CLS& CLS::depth_bias_enable(bool enable) {
        info_.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
        return *this;
    }

    CLS& CLS::depth_bias_constant(float value) {
        info_.depthBiasConstantFactor = value;
        return *this;
    }

    CLS& CLS::depth_bias_slope(float value) {
        info_.depthBiasSlopeFactor = value;
        return *this;
    }

    CLS& CLS::depth_bias(float constant, float slope) {
        info_.depthBiasEnable = VK_TRUE;
        info_.depthBiasConstantFactor = constant;
        info_.depthBiasSlopeFactor = slope;
        return *this;
    }

    CLS& CLS::depth_clamp_enable(bool enable) {
        info_.depthClampEnable = enable ? VK_TRUE : VK_FALSE;
        return *this;
    }

    const VkPipelineRasterizationStateCreateInfo* CLS::get() const {
        return &info_;
    }

#undef CLS

}  // namespace mirinae


// Misc inside PipelineBuilder
namespace mirinae {

    PipelineBuilder::DepthStencilStateBuilder::DepthStencilStateBuilder() {
        info_ = {};
        info_.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        info_.depthTestEnable = VK_FALSE;
        info_.depthWriteEnable = VK_FALSE;
        info_.depthCompareOp = VK_COMPARE_OP_GREATER;
        info_.depthBoundsTestEnable = VK_FALSE;
        info_.stencilTestEnable = VK_FALSE;
        info_.front = {};
        info_.back = {};
        info_.minDepthBounds = 0;
        info_.maxDepthBounds = 1;
    }

    VkPipelineDynamicStateCreateInfo
    PipelineBuilder::DynamicStateBuilder::build() const {
        VkPipelineDynamicStateCreateInfo out{};
        out.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        out.dynamicStateCount = static_cast<uint32_t>(data_.size());
        out.pDynamicStates = data_.data();
        return out;
    }

}  // namespace mirinae


// ColorBlendStateBuilder
namespace mirinae {

#define CLS PipelineBuilder::ColorBlendStateBuilder

    CLS& CLS::add() {
        auto& added = data_.emplace_back();
        added.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                               VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;

        added.blendEnable = VK_FALSE;
        added.colorBlendOp = VK_BLEND_OP_ADD;
        added.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        added.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        added.alphaBlendOp = VK_BLEND_OP_ADD;
        added.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        added.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

        return *this;
    }

    CLS& CLS::duplicate() {
        data_.push_back(data_.back());
        return *this;
    }

    CLS& CLS::duplicate(size_t count) {
        if (count < 1)
            return *this;

        this->duplicate();
        for (size_t i = 1; i < count; ++i) data_.push_back(data_.back());
        return *this;
    }

    CLS& CLS::set_alpha_blend() {
        auto& added = data_.back();
        added.blendEnable = VK_TRUE;
        added.colorBlendOp = VK_BLEND_OP_ADD;
        added.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        added.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        added.alphaBlendOp = VK_BLEND_OP_ADD;
        added.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        added.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        return *this;
    }

    CLS& CLS::set_additive_blend() {
        auto& added = data_.back();
        added.blendEnable = VK_TRUE;
        added.colorBlendOp = VK_BLEND_OP_ADD;
        added.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        added.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        added.alphaBlendOp = VK_BLEND_OP_ADD;
        added.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        added.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        return *this;
    }

    CLS& CLS::add(bool blend_enabled) {
        this->add();
        if (blend_enabled)
            this->set_alpha_blend();
        return *this;
    }

    CLS& CLS::add(bool blend_enabled, size_t count) {
        if (count < 1)
            return *this;

        this->add();
        if (blend_enabled)
            this->set_alpha_blend();
        this->duplicate(count - 1);
        return *this;
    }

    VkPipelineColorBlendStateCreateInfo CLS::build() const {
        VkPipelineColorBlendStateCreateInfo output{};
        output.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
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

#undef CLS

}  // namespace mirinae


// PipelineBuilder
namespace {

    auto create_info_viewport_state() {
        VkPipelineViewportStateCreateInfo output{};
        output.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        output.viewportCount = 1;
        output.pViewports = nullptr;
        output.scissorCount = 1;
        output.pScissors = nullptr;
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

}  // namespace
namespace mirinae {

    PipelineBuilder::PipelineBuilder(mirinae::VulkanDevice& device)
        : device_(device), shader_stages_{ device } {
        viewport_state_ = create_info_viewport_state();
        multisampling_state_ = create_info_multisampling();
    }

    VkPipeline PipelineBuilder::build(
        VkRenderPass render_pass, VkPipelineLayout layout
    ) const {
        const auto vertex_input_state = vertex_input_state_.build();
        const auto color_blending = color_blend_state_.build();
        const auto dynamic_state_info = dynamic_state_.build();

        VkGraphicsPipelineCreateInfo cinfo{};
        cinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        cinfo.stageCount = shader_stages_.size();
        cinfo.pStages = shader_stages_.data();
        cinfo.pVertexInputState = &vertex_input_state;
        cinfo.pInputAssemblyState = input_assembly_state_.get();
        cinfo.pTessellationState = tes_state_.get();
        cinfo.pViewportState = &viewport_state_;
        cinfo.pRasterizationState = rasterization_state_.get();
        cinfo.pMultisampleState = &multisampling_state_;
        cinfo.pDepthStencilState = depth_stencil_state_.get();
        cinfo.pColorBlendState = &color_blending;
        cinfo.pDynamicState = &dynamic_state_info;
        cinfo.layout = layout;
        cinfo.renderPass = render_pass;
        cinfo.subpass = 0;
        cinfo.basePipelineHandle = VK_NULL_HANDLE;
        cinfo.basePipelineIndex = -1;


        VkPipeline out = VK_NULL_HANDLE;
        const auto res = vkCreateGraphicsPipelines(
            device_.logi_device(), VK_NULL_HANDLE, 1, &cinfo, nullptr, &out
        );
        if (VK_SUCCESS != res) {
            MIRINAE_ABORT("failed to create graphics pipeline.");
        }

        return out;
    }

}  // namespace mirinae


// PipelineLayoutBuilder
namespace mirinae {

#define CLS PipelineLayoutBuilder

    PipelineLayoutBuilder& CLS::reset_stage_flags() {
        pc_stage_flags_ = 0;
        return *this;
    }

    PipelineLayoutBuilder& CLS::set_stage_flags(VkShaderStageFlags flags) {
        pc_stage_flags_ = flags;
        return *this;
    }

    PipelineLayoutBuilder& CLS::add_stage_flags(VkShaderStageFlags flags) {
        pc_stage_flags_ |= flags;
        return *this;
    }

    PipelineLayoutBuilder& CLS::add_vertex_flag() {
        return this->add_stage_flags(VK_SHADER_STAGE_VERTEX_BIT);
    }

    PipelineLayoutBuilder& CLS::add_tesc_flag() {
        return this->add_stage_flags(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    }

    PipelineLayoutBuilder& CLS::add_tese_flag() {
        return this->add_stage_flags(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
        );
    }

    PipelineLayoutBuilder& CLS::add_frag_flag() {
        return this->add_stage_flags(VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    PipelineLayoutBuilder& CLS::pc(uint32_t offset, uint32_t size) {
        auto& added = pc_ranges_.emplace_back();
        added.stageFlags = pc_stage_flags_;
        added.offset = offset;
        added.size = size;
        return *this;
    }

    PipelineLayoutBuilder& CLS::desc(VkDescriptorSetLayout layout) {
        desclayouts_.push_back(layout);
        return *this;
    }

    VkPipelineLayout CLS::build(mirinae::VulkanDevice& device) {
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

    void CLS::build(VkPipelineLayout& out, mirinae::VulkanDevice& device) {
        out = this->build(device);
    }

#undef CLS

}  // namespace mirinae
