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

}  // namespace mirinae
