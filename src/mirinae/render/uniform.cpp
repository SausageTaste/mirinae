#include "mirinae/render/uniform.hpp"

#include <stdexcept>


// DescriptorSetLayout
namespace mirinae {

    void DescriptorSetLayout::init(LogiDevice& logi_device) {
        this->destroy(logi_device);

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(logi_device.get(), &layoutInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void DescriptorSetLayout::destroy(LogiDevice& logi_device) {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}
