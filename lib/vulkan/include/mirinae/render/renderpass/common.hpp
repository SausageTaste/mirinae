#pragma once

#include <map>

#include "mirinae/render/renderee.hpp"
#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class RenderPassBuilder {

    public:
        class AttachDescView {

        public:
            AttachDescView(VkAttachmentDescription& desc);
            ~AttachDescView();

            AttachDescView& format(VkFormat x);
            AttachDescView& samples(VkSampleCountFlagBits x);

            AttachDescView& load_op(VkAttachmentLoadOp x);
            AttachDescView& stor_op(VkAttachmentStoreOp x);
            AttachDescView& op_pair_clear_store();
            AttachDescView& op_pair_load_store();

            AttachDescView& stencil_load(VkAttachmentLoadOp x);
            AttachDescView& stencil_stor(VkAttachmentStoreOp x);

            AttachDescView& ini_layout(VkImageLayout x);
            AttachDescView& fin_layout(VkImageLayout x);

            AttachDescView& preset_default();

        private:
            VkAttachmentDescription& desc_;
        };


        class AttachDescBuilder {

        public:
            const VkAttachmentDescription* data() const;
            uint32_t size() const;

            AttachDescView add(const VkFormat format);
            AttachDescView dup(const VkFormat format);

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

            ShaderStagesBuilder& add(
                const mirinae::respath_t& spv_path,
                const VkShaderStageFlagBits stage
            );

            ShaderStagesBuilder& add_vert(const mirinae::respath_t& spv_path);
            ShaderStagesBuilder& add_frag(const mirinae::respath_t& spv_path);
            ShaderStagesBuilder& add_tesc(const mirinae::respath_t& spv_path);
            ShaderStagesBuilder& add_tese(const mirinae::respath_t& spv_path);

            const VkPipelineShaderStageCreateInfo* data() const;
            uint32_t size() const;

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


        class InputAssemblyStateBuilder {

        public:
            InputAssemblyStateBuilder();

            InputAssemblyStateBuilder& topology(VkPrimitiveTopology top);
            InputAssemblyStateBuilder& topology_patch_list();

            const VkPipelineInputAssemblyStateCreateInfo* get() const {
                return &info_;
            }

        private:
            VkPipelineInputAssemblyStateCreateInfo info_;
        };


        class TessellationStateBuilder {

        public:
            TessellationStateBuilder();
            TessellationStateBuilder& patch_ctrl_points(uint32_t count);
            const VkPipelineTessellationStateCreateInfo* get() const;

        private:
            VkPipelineTessellationStateCreateInfo info_ = {};
            bool enabled_ = false;
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

            RasterizationStateBuilder& depth_bias(float constant, float slope) {
                info_.depthBiasEnable = VK_TRUE;
                info_.depthBiasConstantFactor = constant;
                info_.depthBiasSlopeFactor = slope;
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
                return this->add(VK_DYNAMIC_STATE_VIEWPORT);
            }
            DynamicStateBuilder& add_scissor() {
                return this->add(VK_DYNAMIC_STATE_SCISSOR);
            }

            VkPipelineDynamicStateCreateInfo build() const;

        private:
            std::vector<VkDynamicState> data_;
        };


        PipelineBuilder(mirinae::VulkanDevice& device);

        auto& shader_stages() { return shader_stages_; }
        auto& vertex_input_state() { return vertex_input_state_; }
        auto& input_assembly_state() { return input_assembly_state_; }
        auto& tes_state() { return tes_state_; }
        auto& rasterization_state() { return rasterization_state_; }
        auto& depth_stencil_state() { return depth_stencil_state_; }
        auto& color_blend_state() { return color_blend_state_; }
        auto& dynamic_state() { return dynamic_state_; }

        VkPipeline build(VkRenderPass rp, VkPipelineLayout layout) const;

    private:
        mirinae::VulkanDevice& device_;
        ShaderStagesBuilder shader_stages_;
        VertexInputStateBuilder vertex_input_state_;
        InputAssemblyStateBuilder input_assembly_state_;
        TessellationStateBuilder tes_state_;
        VkPipelineViewportStateCreateInfo viewport_state_;
        RasterizationStateBuilder rasterization_state_;
        VkPipelineMultisampleStateCreateInfo multisampling_state_;
        DepthStencilStateBuilder depth_stencil_state_;
        ColorBlendStateBuilder color_blend_state_;
        DynamicStateBuilder dynamic_state_;
    };


    class PipelineLayoutBuilder {

    public:
        PipelineLayoutBuilder& reset_stage_flags(VkShaderStageFlags flags);
        PipelineLayoutBuilder& add_vertex_flag();
        PipelineLayoutBuilder& add_frag_flag();

        PipelineLayoutBuilder& pc(uint32_t offset, uint32_t size);

        template <typename T>
        PipelineLayoutBuilder& pc(uint32_t offset = 0) {
            return this->pc(offset, sizeof(T));
        }

        PipelineLayoutBuilder& desc(VkDescriptorSetLayout layout);

        VkPipelineLayout build(mirinae::VulkanDevice& device);

    private:
        std::vector<VkDescriptorSetLayout> desclayouts_;
        std::vector<VkPushConstantRange> pc_ranges_;
        VkShaderStageFlags pc_stage_flags_ = 0;
    };


    class FbufImageBundle {

    public:
        void init(
            uint32_t width, uint32_t height, mirinae::TextureManager& tex_man
        ) {
            depth_ = tex_man.create_depth(width, height);
            albedo_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "albedo"
            );
            normal_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "normal"
            );
            material_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "material"
            );
            compo_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                mirinae::FbufUsage::color_attachment,
                "compo"
            );
        }

        uint32_t width() const { return depth_->width(); }
        uint32_t height() const { return depth_->height(); }
        VkExtent2D extent() const { return { this->width(), this->height() }; }

        mirinae::ITexture& depth() { return *depth_; }
        mirinae::ITexture& albedo() { return *albedo_; }
        mirinae::ITexture& normal() { return *normal_; }
        mirinae::ITexture& material() { return *material_; }
        mirinae::ITexture& compo() { return *compo_; }

    private:
        std::unique_ptr<mirinae::ITexture> depth_;
        std::unique_ptr<mirinae::ITexture> albedo_;
        std::unique_ptr<mirinae::ITexture> normal_;
        std::unique_ptr<mirinae::ITexture> material_;
        std::unique_ptr<mirinae::ITexture> compo_;
    };


    class IRenderPassBundle {

    public:
        IRenderPassBundle(mirinae::VulkanDevice& device) : device_(device) {}

        virtual ~IRenderPassBundle() = default;
        virtual void destroy() = 0;

        virtual VkFramebuffer fbuf_at(uint32_t index) const = 0;
        virtual const VkClearValue* clear_values() const = 0;
        virtual uint32_t clear_value_count() const = 0;

        VkRenderPass renderpass() const { return renderpass_; }
        VkPipeline pipeline() const { return pipeline_; }
        VkPipelineLayout pipeline_layout() const { return layout_; }

    protected:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
    };


    using RpMap = std::map<std::string, std::unique_ptr<IRenderPassBundle>>;

    void create_rp_gbuf(
        RpMap& out,
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

    void create_rp_envmap(
        RpMap& out, DesclayoutManager& desclayouts, VulkanDevice& device
    );

    void create_rp_shadow(
        RpMap& out,
        VkFormat depth_format,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );

}  // namespace mirinae
