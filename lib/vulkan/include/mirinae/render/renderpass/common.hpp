#pragma once

#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class RenderPassBuilder {

    public:
        class AttachDescView {

        public:
            AttachDescView(VkAttachmentDescription& desc) : desc_{ desc } {}

            ~AttachDescView() = default;

            AttachDescView& format(VkFormat x) {
                desc_.format = x;
                return *this;
            }

            AttachDescView& samples(VkSampleCountFlagBits x) {
                desc_.samples = x;
                return *this;
            }

            AttachDescView& load_op(VkAttachmentLoadOp x) {
                desc_.loadOp = x;
                return *this;
            }

            AttachDescView& stor_op(VkAttachmentStoreOp x) {
                desc_.storeOp = x;
                return *this;
            }

            AttachDescView& op_pair_clear_store() {
                desc_.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                desc_.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                return *this;
            }

            AttachDescView& op_pair_load_store() {
                desc_.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                desc_.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                return *this;
            }

            AttachDescView& stencil_load(VkAttachmentLoadOp x) {
                desc_.stencilLoadOp = x;
                return *this;
            }

            AttachDescView& stencil_stor(VkAttachmentStoreOp x) {
                desc_.stencilStoreOp = x;
                return *this;
            }

            AttachDescView& ini_layout(VkImageLayout x) {
                desc_.initialLayout = x;
                return *this;
            }

            AttachDescView& fin_layout(VkImageLayout x) {
                desc_.finalLayout = x;
                return *this;
            }

            AttachDescView& preset_default() {
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

        private:
            VkAttachmentDescription& desc_;
        };


        class AttachDescBuilder {

        public:
            const VkAttachmentDescription* data() const {
                if (data_.empty())
                    return nullptr;
                else
                    return data_.data();
            }

            uint32_t size() const {
                return static_cast<uint32_t>(data_.size());
            }

            AttachDescView add(const VkFormat format) {
                auto& added = data_.emplace_back();
                AttachDescView view{ added };
                view.preset_default();
                view.format(format);
                return view;
            }

            AttachDescView dup(const VkFormat format) {
                const auto& last = data_.back();
                auto& added = data_.emplace_back(last);
                AttachDescView view{ added };
                view.format(format);
                return view;
            }

        private:
            std::vector<VkAttachmentDescription> data_;
        };


        class AttachRefBuilder {

        public:
            const VkAttachmentReference* data() const {
                if (data_.empty())
                    return nullptr;
                else
                    return data_.data();
            }

            uint32_t size() const {
                return static_cast<uint32_t>(data_.size());
            }

            AttachRefBuilder& add(uint32_t index, VkImageLayout layout) {
                auto& added = data_.emplace_back();
                added = {};
                added.attachment = index;
                added.layout = layout;
                return *this;
            }

            AttachRefBuilder& add_color_attach(uint32_t index) {
                return this->add(
                    index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                );
            }

        private:
            std::vector<VkAttachmentReference> data_;
        };


        class DepthAttachRefBuilder {

        public:
            const VkAttachmentReference* data() const {
                if (data_.has_value())
                    return &data_.value();
                else
                    return nullptr;
            }

            void set(uint32_t index, VkImageLayout layout) {
                data_ = VkAttachmentReference{};
                data_->attachment = index;
                data_->layout = layout;
            }

            void set(uint32_t index) {
                this->set(
                    index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                );
            }

            void clear() { data_.reset(); }

        private:
            std::optional<VkAttachmentReference> data_;
        };


        class SubpassDepView {

        public:
            SubpassDepView(VkSubpassDependency& data) : data_{ data } {}

            ~SubpassDepView() = default;

            SubpassDepView& clear() {
                data_ = {};
                return *this;
            }

            SubpassDepView& preset_single() {
                data_ = {};

                data_.srcSubpass = VK_SUBPASS_EXTERNAL;
                data_.dstSubpass = 0;
                data_.srcStageMask =
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                data_.srcAccessMask = 0;
                data_.dstStageMask =
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                data_.dstAccessMask =
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                return *this;
            }

        private:
            VkSubpassDependency& data_;
        };


        class SubpassDepBuilder {

        public:
            SubpassDepView add() {
                auto& dependency = data_.emplace_back();
                SubpassDepView view{ dependency };
                view.clear();
                return view;
            }

            const VkSubpassDependency* data() const {
                if (data_.empty())
                    return nullptr;
                else
                    return data_.data();
            }

            uint32_t size() const {
                return static_cast<uint32_t>(data_.size());
            }

        private:
            std::vector<VkSubpassDependency> data_;
        };


        AttachDescBuilder& attach_desc() { return attach_desc_; }
        AttachRefBuilder& color_attach_ref() { return color_attach_ref_; }
        DepthAttachRefBuilder& depth_attach_ref() { return depth_attach_ref_; }
        SubpassDepBuilder& subpass_dep() { return subpass_dep_; }

        VkRenderPass build(VkDevice logi_device);

    private:
        AttachDescBuilder attach_desc_;
        AttachRefBuilder color_attach_ref_;
        DepthAttachRefBuilder depth_attach_ref_;
        SubpassDepBuilder subpass_dep_;
    };


    class PipelineBuilder {

    public:
        class ShaderStagesBuilder {

        public:
            ShaderStagesBuilder(mirinae::VulkanDevice& device);
            ~ShaderStagesBuilder();

            ShaderStagesBuilder& add_vert(const mirinae::respath_t& spv_path);
            ShaderStagesBuilder& add_frag(const mirinae::respath_t& spv_path);

            const VkPipelineShaderStageCreateInfo* data() const {
                return stages_.data();
            }
            uint32_t size() const {
                return static_cast<uint32_t>(stages_.size());
            }

        private:
            void add_stage(VkShaderStageFlagBits stage, VkShaderModule module);

            static VkShaderModule load_spv(
                const mirinae::respath_t& spv_path,
                mirinae::VulkanDevice& device
            );

            static std::optional<VkShaderModule> create_shader_module(
                const std::vector<uint8_t>& spv, VkDevice logi_device
            );

            mirinae::VulkanDevice& device_;
            std::vector<VkPipelineShaderStageCreateInfo> stages_;
            std::vector<VkShaderModule> modules_;
        };


        class VertexInputStateBuilder {

        public:
            VertexInputStateBuilder& set_static();
            VertexInputStateBuilder& set_skinned();

            VkPipelineVertexInputStateCreateInfo build() const;

        private:
            void add_attrib(VkFormat format, uint32_t offset);

            void add_attrib_vec2(uint32_t offset);
            void add_attrib_vec3(uint32_t offset);
            void add_attrib_vec4(uint32_t offset);
            void add_attrib_ivec4(uint32_t offset);

            void set_binding_static();
            void set_binding_skinned();
            void set_attrib_static();
            void set_attrib_skinned();

            std::vector<VkVertexInputBindingDescription> bindings_;
            std::vector<VkVertexInputAttributeDescription> attribs_;
        };


        class RasterizationStateBuilder {

        public:
            RasterizationStateBuilder();

            RasterizationStateBuilder& cull_mode(VkCullModeFlags mode) {
                info_.cullMode = mode;
                return *this;
            }

            RasterizationStateBuilder& cull_mode_back() {
                info_.cullMode = VK_CULL_MODE_BACK_BIT;
                return *this;
            }

            RasterizationStateBuilder& depth_bias_enable(bool enable) {
                info_.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
                return *this;
            }

            RasterizationStateBuilder& depth_bias_constant(float value) {
                info_.depthBiasConstantFactor = value;
                return *this;
            }

            RasterizationStateBuilder& depth_bias_slope(float value) {
                info_.depthBiasSlopeFactor = value;
                return *this;
            }

            RasterizationStateBuilder& depth_clamp_enable(bool enable) {
                info_.depthClampEnable = enable ? VK_TRUE : VK_FALSE;
                return *this;
            }

            const VkPipelineRasterizationStateCreateInfo* get() const {
                return &info_;
            }

        private:
            VkPipelineRasterizationStateCreateInfo info_;
        };


        class DepthStencilStateBuilder {

        public:
            DepthStencilStateBuilder();

            DepthStencilStateBuilder& depth_test_enable(bool enable) {
                info_.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
                return *this;
            }

            DepthStencilStateBuilder& depth_write_enable(bool enable) {
                info_.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
                return *this;
            }

            const VkPipelineDepthStencilStateCreateInfo* get() const {
                return &info_;
            }

        private:
            VkPipelineDepthStencilStateCreateInfo info_;
        };


        class ColorBlendStateBuilder {

        public:
            ColorBlendStateBuilder& add(bool blend_enabled);
            ColorBlendStateBuilder& add(bool blend_enabled, size_t count);

            VkPipelineColorBlendStateCreateInfo build() const;

        private:
            std::vector<VkPipelineColorBlendAttachmentState> data_;
        };


        class DynamicStateBuilder {

        public:
            DynamicStateBuilder& add(VkDynamicState state) {
                data_.push_back(state);
                return *this;
            }
            DynamicStateBuilder& add_viewport() {
                data_.push_back(VK_DYNAMIC_STATE_VIEWPORT);
                return *this;
            }
            DynamicStateBuilder& add_scissor() {
                data_.push_back(VK_DYNAMIC_STATE_SCISSOR);
                return *this;
            }

            VkPipelineDynamicStateCreateInfo build() const;

        private:
            std::vector<VkDynamicState> data_;
        };


        PipelineBuilder(mirinae::VulkanDevice& device);

        auto& shader_stages() { return shader_stages_; }
        auto& vertex_input_state() { return vertex_input_state_; }
        auto& rasterization_state() { return rasterization_state_; }
        auto& depth_stencil_state() { return depth_stencil_state_; }
        auto& color_blend_state() { return color_blend_state_; }
        auto& dynamic_state() { return dynamic_state_; }

        VkPipeline build(VkRenderPass rp, VkPipelineLayout layout) const;

    private:
        mirinae::VulkanDevice& device_;
        ShaderStagesBuilder shader_stages_;
        VertexInputStateBuilder vertex_input_state_;
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_;
        VkPipelineViewportStateCreateInfo viewport_state_;
        RasterizationStateBuilder rasterization_state_;
        VkPipelineMultisampleStateCreateInfo multisampling_state_;
        DepthStencilStateBuilder depth_stencil_state_;
        ColorBlendStateBuilder color_blend_state_;
        DynamicStateBuilder dynamic_state_;
    };

}  // namespace mirinae