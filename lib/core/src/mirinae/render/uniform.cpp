#include "mirinae/render/uniform.hpp"

#include <stdexcept>
#include <vector>


// DescLayoutBuilder
namespace {

    class DescLayoutBuilder {

    public:
        void add_uniform_buffer(VkShaderStageFlagBits stage_flags, uint32_t count) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++uniform_buffer_count_;
        }

        void add_combined_image_sampler(VkShaderStageFlagBits stage_flags, uint32_t count) {
            auto& binding = bindings_.emplace_back();
            binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = count;
            binding.stageFlags = stage_flags;
            binding.pImmutableSamplers = nullptr;

            ++combined_image_sampler_count_;
        }

        std::optional<VkDescriptorSetLayout> build(VkDevice logi_device) const {
            VkDescriptorSetLayoutCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            create_info.bindingCount = static_cast<uint32_t>(bindings_.size());
            create_info.pBindings = bindings_.data();

            VkDescriptorSetLayout handle;
            if (VK_SUCCESS != vkCreateDescriptorSetLayout(logi_device, &create_info, nullptr, &handle))
                return std::nullopt;

            return handle;
        }

        auto ubuf_count() const { return uniform_buffer_count_; }
        auto img_sampler_count() const { return combined_image_sampler_count_; }

    public:
        std::vector<VkDescriptorSetLayoutBinding> bindings_;
        size_t uniform_buffer_count_ = 0;
        size_t combined_image_sampler_count_ = 0;

    };

}


// DescLayout
namespace mirinae {

    DescLayout DescLayout::create_model(VulkanDevice& device) {
        DescLayoutBuilder builder;
        builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1);

        if (auto handle = builder.build(device.logi_device()))
            return DescLayout(handle.value(), device);
        else
            throw std::runtime_error("Failed to create descriptor set layout: model");
    }

    DescLayout DescLayout::create_actor(VulkanDevice& device) {
        DescLayoutBuilder builder;
        builder.add_uniform_buffer(VK_SHADER_STAGE_VERTEX_BIT, 1);

        if (auto handle = builder.build(device.logi_device()))
            return DescLayout(handle.value(), device);
        else
            throw std::runtime_error("Failed to create descriptor set layout: actor");
    }

    void DescLayout::destroy() {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_.logi_device(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    DescLayout::DescLayout(VkDescriptorSetLayout handle, VulkanDevice& device)
        : handle_(handle)
        , device_(device)
    {

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
