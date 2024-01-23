#pragma once

#include <list>

#include "mirinae/util/include_glm.hpp"

#include "vkmajorplayers.hpp"


namespace mirinae {

    struct U_GbufActor {
        glm::mat4 view_model;
        glm::mat4 pvm;
    };


    struct U_CompositionMain {
        glm::mat4 proj_inv;
    };


    class U_OverlayMain {

    public:
        glm::vec2& size() {
            return size_;
        }
        glm::vec2& offset() {
            return offset_;
        }

    private:
        glm::vec2 size_;
        glm::vec2 offset_;

    };
    static_assert(sizeof(U_OverlayMain) == 16);


    class DesclayoutManager {

    public:
        DesclayoutManager(VulkanDevice& device);
        ~DesclayoutManager();

        void add(const std::string& name, VkDescriptorSetLayout handle);
        VkDescriptorSetLayout get(const std::string& name);

    private:
        class Item;
        std::vector<Item> data_;
        VulkanDevice& device_;

    };


    class DescWriteInfoBuilder {

    public:
        DescWriteInfoBuilder& add_uniform_buffer(const mirinae::Buffer& buffer, VkDescriptorSet descset);
        DescWriteInfoBuilder& add_combinded_image_sampler(VkImageView image_view, VkSampler sampler, VkDescriptorSet descset);
        DescWriteInfoBuilder& add_input_attachment(VkImageView image_view, VkDescriptorSet descset);

        void apply_all(VkDevice logi_device);

    private:
        std::vector<VkWriteDescriptorSet> data_;
        std::list<VkDescriptorBufferInfo> buffer_info_;
        std::list<VkDescriptorImageInfo> image_info_;

    };


    class DescriptorPool {

    public:
        void init(uint32_t pool_size, VkDevice logi_device);
        void destroy(VkDevice logi_device);

         std::vector<VkDescriptorSet> alloc(uint32_t count, VkDescriptorSetLayout desclayout, VkDevice logi_device);

    private:
        VkDescriptorPool handle_ = VK_NULL_HANDLE;

    };

}
