#include "mirinae/render/uniform.hpp"

#include <stdexcept>
#include <vector>

#include <spdlog/spdlog.h>


// DescSizeInfo
namespace mirinae {

    uint32_t DescSizeInfo::get(VkDescriptorType type) const {
        const auto it = data_.find(type);
        if (it == data_.end())
            return 0;
        else
            return it->second;
    }

    void DescSizeInfo::set(VkDescriptorType type, uint32_t cnt) {
        data_[type] = cnt;
    }

    void DescSizeInfo::add(VkDescriptorType type, uint32_t cnt) {
        const auto it = data_.find(type);
        if (it == data_.end())
            data_[type] = cnt;
        else
            it->second += cnt;
    }

}  // namespace mirinae


// DescLayoutBuilder
namespace mirinae {

    DescLayoutBuilder::DescLayoutBuilder(const char* name) : name_(name) {}

    DescLayoutBuilder& DescLayoutBuilder::add_ubuf(
        VkShaderStageFlagBits stage_flags, uint32_t count
    ) {
        auto& binding = bindings_.emplace_back();
        binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = count;
        binding.stageFlags = stage_flags;
        binding.pImmutableSamplers = nullptr;

        size_info_.add_ubuf(count);
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::add_img(
        VkShaderStageFlagBits stage_flags, uint32_t count
    ) {
        auto& binding = bindings_.emplace_back();
        binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = count;
        binding.stageFlags = stage_flags;
        binding.pImmutableSamplers = nullptr;

        size_info_.add_img(count);
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::add_input_att(
        VkShaderStageFlagBits stage_flags, uint32_t count
    ) {
        auto& binding = bindings_.emplace_back();
        binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        binding.descriptorCount = count;
        binding.stageFlags = stage_flags;
        binding.pImmutableSamplers = nullptr;

        size_info_.add_input_att(count);
        return *this;
    }

    VkDescriptorSetLayout DescLayoutBuilder::build(VkDevice logi_device) const {
        VkDescriptorSetLayoutCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        create_info.bindingCount = static_cast<uint32_t>(bindings_.size());
        create_info.pBindings = bindings_.data();

        VkDescriptorSetLayout handle;
        if (vkCreateDescriptorSetLayout(
                logi_device, &create_info, nullptr, &handle
            ) != VK_SUCCESS) {
            throw std::runtime_error{
                fmt::format("Failed to create descriptor set layout: {}", name_)
            };
        }

        return handle;
    }

}  // namespace mirinae


// DesclayoutManager
namespace mirinae {

    class DesclayoutManager::Item {

    public:
        void destroy(VkDevice logi_device) {
            vkDestroyDescriptorSetLayout(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
            name_.clear();
        }

        DescSizeInfo size_info_;
        std::string name_;
        VkDescriptorSetLayout handle_;
    };


    DesclayoutManager::DesclayoutManager(VulkanDevice& device)
        : device_(device) {}

    DesclayoutManager::~DesclayoutManager() {
        for (auto& item : data_) item.destroy(device_.logi_device());
        data_.clear();
    }

    VkDescriptorSetLayout DesclayoutManager::add(
        const DescLayoutBuilder& builder, VkDevice logi_device
    ) {
        auto& item = data_.emplace_back();
        item.size_info_ = builder.size_info();
        item.name_ = builder.name();
        item.handle_ = builder.build(logi_device);
        return item.handle_;
    }

    VkDescriptorSetLayout DesclayoutManager::get(const std::string& name) {
        for (auto& item : data_) {
            if (item.name_ == name)
                return item.handle_;
        }

        throw std::runtime_error{
            fmt::format("Failed to find descriptor set layout: {}", name)
        };
    }

    const DescSizeInfo& DesclayoutManager::get_size_info(
        const std::string& desc_name
    ) const {
        for (const auto& item : data_) {
            if (item.name_ == desc_name)
                return item.size_info_;
        }

        throw std::runtime_error{
            fmt::format("Failed to find descriptor set layout: {}", desc_name)
        };
    }

}  // namespace mirinae


// DescWriteInfoBuilder
namespace mirinae {

    DescWriteInfoBuilder& DescWriteInfoBuilder::set_descset(
        VkDescriptorSet descset
    ) {
        descset_ = descset;
        binding_index_ = 0;
        return *this;
    }

    DescWriteInfoBuilder& DescWriteInfoBuilder::add_ubuf(
        const mirinae::Buffer& buffer
    ) {
        auto& buffer_info = buffer_info_.emplace_back();
        buffer_info.buffer = buffer.buffer();
        buffer_info.offset = 0;
        buffer_info.range = buffer.size();

        auto& write = data_.emplace_back();
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descset_;
        write.dstBinding = binding_index_++;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &buffer_info;

        return *this;
    }

    DescWriteInfoBuilder& DescWriteInfoBuilder::add_img_sampler(
        VkImageView image_view, VkSampler sampler
    ) {
        auto& image_info = image_info_.emplace_back();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = image_view;
        image_info.sampler = sampler;

        auto& write = data_.emplace_back();
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descset_;
        write.dstBinding = binding_index_++;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &image_info;

        return *this;
    }

    DescWriteInfoBuilder& DescWriteInfoBuilder::add_input_attach(
        VkImageView image_view
    ) {
        auto& image_info = image_info_.emplace_back();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = image_view;
        image_info.sampler = VK_NULL_HANDLE;

        auto& write = data_.emplace_back();
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descset_;
        write.dstBinding = binding_index_++;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        write.descriptorCount = 1;
        write.pImageInfo = &image_info;

        return *this;
    }

    void DescWriteInfoBuilder::apply_all(VkDevice logi_device) {
        vkUpdateDescriptorSets(
            logi_device, data_.size(), data_.data(), 0, nullptr
        );
    }

}  // namespace mirinae


// DescPool
namespace mirinae {

    // basic implementation for DescPool methods
    void DescPool::init(uint32_t alloc_size, VkDevice logi_device) {
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

        VK_CHECK(
            vkCreateDescriptorPool(logi_device, &poolInfo, nullptr, &handle_)
        );
    }

    // For rest of methods too
    void DescPool::destroy(VkDevice logi_device) {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    std::vector<VkDescriptorSet> DescPool::alloc(
        uint32_t count, VkDescriptorSetLayout desclayout, VkDevice logi_device
    ) {
        std::vector<VkDescriptorSetLayout> layouts(count, desclayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = handle_;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> output(count);
        VK_CHECK(
            vkAllocateDescriptorSets(logi_device, &allocInfo, output.data())
        );

        return output;
    }

}  // namespace mirinae
