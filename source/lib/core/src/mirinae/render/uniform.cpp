#include "mirinae/render/uniform.hpp"

#include <stdexcept>
#include <vector>


// DescriptorSetLayout
namespace mirinae {

    void DescriptorSetLayout::init(VkDevice logi_device) {
        this->destroy(logi_device);

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        {
            auto& binding = bindings.emplace_back();
            binding.binding = bindings.size() - 1;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            binding.pImmutableSamplers = nullptr;
        }
        {
            auto& binding = bindings.emplace_back();
            binding.binding = bindings.size() - 1;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding.pImmutableSamplers = nullptr;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(logi_device, &layoutInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void DescriptorSetLayout::destroy(VkDevice logi_device) {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// DescriptorPool
namespace mirinae {

    //basic implementation for DescriptorPool methods
    void DescriptorPool::init(uint32_t alloc_size, VkDevice logi_device) {
        this->destroy(logi_device);

        std::vector<VkDescriptorPoolSize> poolSizes;
        {
            auto& poolSize = poolSizes.emplace_back();
            poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSize.descriptorCount = alloc_size;
        }
        {
            auto& poolSize = poolSizes.emplace_back();
            poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSize.descriptorCount = alloc_size;
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = alloc_size;

        if (vkCreateDescriptorPool(logi_device, &poolInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    // For rest of methods too
    void DescriptorPool::destroy(VkDevice logi_device) {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    std::vector<VkDescriptorSet> DescriptorPool::alloc(uint32_t count, DescriptorSetLayout& desclayout, VkDevice logi_device) {
        std::vector<VkDescriptorSetLayout> layouts(count, desclayout.get());

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = handle_;
        allocInfo.descriptorSetCount = layouts.size();
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> output(count);
        if (vkAllocateDescriptorSets(logi_device, &allocInfo, output.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor set!");
        }

        return output;
    }

}
