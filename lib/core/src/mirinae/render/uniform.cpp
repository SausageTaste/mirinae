#include "mirinae/render/uniform.hpp"

#include <stdexcept>
#include <vector>

#include <spdlog/spdlog.h>


// DesclayoutManager
namespace mirinae {

    class DesclayoutManager::Item {

    public:
        Item(const char* name, VkDescriptorSetLayout handle)
            : name_(name)
            , handle_(handle)
        {

        }

        void destroy(VkDevice logi_device) {
            vkDestroyDescriptorSetLayout(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
            name_.clear();
        }

        std::string name_;
        VkDescriptorSetLayout handle_;

    };


    DesclayoutManager::DesclayoutManager(VulkanDevice& device) : device_(device) {}

    DesclayoutManager::~DesclayoutManager() {
        for (auto& item : data_)
            item.destroy(device_.logi_device());
        data_.clear();
    }

    VkDescriptorSetLayout DesclayoutManager::add(const char* name, VkDescriptorSetLayout handle) {
        return data_.emplace_back(name, handle).handle_;
    }

    VkDescriptorSetLayout DesclayoutManager::get(const std::string& name) {
        for (auto& item : data_) {
            if (item.name_ == name)
                return item.handle_;
        }

        throw std::runtime_error{ fmt::format("Failed to find descriptor set layout: {}", name) };
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
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
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

    std::vector<VkDescriptorSet> DescriptorPool::alloc(uint32_t count, VkDescriptorSetLayout desclayout, VkDevice logi_device) {
        std::vector<VkDescriptorSetLayout> layouts(count, desclayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = handle_;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> output(count);
        if (vkAllocateDescriptorSets(logi_device, &allocInfo, output.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor set!");
        }

        return output;
    }

}
