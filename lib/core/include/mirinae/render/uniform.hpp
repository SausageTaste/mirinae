#pragma once

#include "mirinae/util/include_glm.hpp"

#include "vkmajorplayers.hpp"


namespace mirinae {

    struct U_Unorthodox {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
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


    struct DescLayoutBundle {
        DescLayoutBundle(VulkanDevice& device)
            : model_(DescLayout::create_model(device))
            , actor_(DescLayout::create_actor(device))
        {

        }

        DescLayout model_;
        DescLayout actor_;
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
