#pragma once

#include "mirinae/util/include_glm.hpp"

#include "vkmajorplayers.hpp"


namespace mirinae {

    struct U_Unorthodox {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };


    class DescriptorSetLayout {

    public:
        void init(VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkDescriptorSetLayout get() { return handle_; }

    private:
        VkDescriptorSetLayout handle_ = VK_NULL_HANDLE;

    };


    class DescLayout {

    public:
        static DescLayout create_model(VulkanDevice& device);
        static DescLayout create_actor(VulkanDevice& device);

        ~DescLayout() {
            this->destroy();
        }

        void destroy();
        VkDescriptorSetLayout get() { return handle_; }

    private:
        DescLayout(VkDescriptorSetLayout handle, VulkanDevice& device);

        VulkanDevice& device_;
        VkDescriptorSetLayout handle_ = VK_NULL_HANDLE;

    };


    class DescriptorPool {

    public:
        void init(uint32_t pool_size, VkDevice logi_device);
        void destroy(VkDevice logi_device);

         std::vector<VkDescriptorSet> alloc(uint32_t count, DescriptorSetLayout& desclayout, VkDevice logi_device);

    private:
        VkDescriptorPool handle_ = VK_NULL_HANDLE;

    };

}
