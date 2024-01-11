#pragma once

#include "mirinae/util/include_glm.hpp"

#include "vkmajorplayers.hpp"


namespace mirinae {

    struct U_Unorthodox {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };


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
        static VkDescriptorBufferInfo create_buffer_info(const mirinae::Buffer& buffer);
        static VkDescriptorImageInfo create_image_info(VkImageView image_view, VkSampler sampler);

        void add(const VkDescriptorBufferInfo& buffer_info, VkDescriptorSet descset);
        void add(const VkDescriptorImageInfo& image_info, VkDescriptorSet descset);

        const VkWriteDescriptorSet* data() const { return data_.data(); }
        uint32_t size() const { return static_cast<uint32_t>(data_.size()); }

    private:
        std::vector<VkWriteDescriptorSet> data_;

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
