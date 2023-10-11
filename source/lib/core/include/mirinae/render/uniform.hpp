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
        void init(LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkDescriptorSetLayout get() { return handle_; }

    private:
        VkDescriptorSetLayout handle_ = VK_NULL_HANDLE;

    };


    class DescriptorPool {

    public:
        void init(uint32_t pool_size, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

         std::vector<VkDescriptorSet> alloc(uint32_t count, DescriptorSetLayout& desclayout, LogiDevice& logi_device);

    private:
        VkDescriptorPool handle_ = VK_NULL_HANDLE;

    };

}
