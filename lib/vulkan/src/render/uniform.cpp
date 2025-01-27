#include "mirinae/render/uniform.hpp"

#include <stdexcept>
#include <vector>

#include "mirinae/lightweight/include_spdlog.hpp"


// DescSizeInfo
namespace mirinae {

    DescSizeInfo DescSizeInfo::operator+(const DescSizeInfo& rhs) const {
        DescSizeInfo output;
        for (const auto& [type, cnt] : data_) output.set(type, cnt);
        for (const auto& [type, cnt] : rhs.data_) output.add(type, cnt);
        return output;
    }

    DescSizeInfo& DescSizeInfo::add(VkDescriptorType type, uint32_t cnt) {
        const auto it = data_.find(type);
        if (it == data_.end())
            data_[type] = cnt;
        else
            it->second += cnt;
        return *this;
    }

    void DescSizeInfo::multiply_counts(uint32_t factor) {
        for (auto& [type, cnt] : data_) cnt *= factor;
    }

    std::vector<VkDescriptorPoolSize> DescSizeInfo::create_arr() const {
        std::vector<VkDescriptorPoolSize> output;
        for (const auto& [type, cnt] : data_) {
            auto& pool_size = output.emplace_back();
            pool_size.type = type;
            pool_size.descriptorCount = cnt;
        }
        return output;
    }

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

}  // namespace mirinae


// DescLayoutBuilder
namespace mirinae {

    DescLayoutBuilder::DescLayoutBuilder(const char* name) : name_(name) {}

    DescLayoutBuilder::DescLayoutBuilder(const std::string& name)
        : name_(name) {}

    DescLayoutBuilder& DescLayoutBuilder::new_binding() {
        auto& binding = bindings_.emplace_back();
        binding.binding = static_cast<uint32_t>(bindings_.size() - 1);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        binding.descriptorCount = 0;
        binding.stageFlags = 0;
        binding.pImmutableSamplers = nullptr;
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::set_type(VkDescriptorType type) {
        bindings_.back().descriptorType = type;
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::set_count(uint32_t cnt) {
        bindings_.back().descriptorCount = cnt;
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::set_stage(VkShaderStageFlags stage) {
        bindings_.back().stageFlags = stage;
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::add_stage(VkShaderStageFlags stage) {
        bindings_.back().stageFlags |= stage;
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::finish_binding() {
        auto& binding = bindings_.back();
        size_info_.add(binding.descriptorType, binding.descriptorCount);
        return *this;
    }

    DescLayoutBuilder& DescLayoutBuilder::add_ubuf(
        VkShaderStageFlags stage_flags, uint32_t count
    ) {
        return this->new_binding()
            .set_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .set_count(count)
            .set_stage(stage_flags)
            .finish_binding();
    }

    DescLayoutBuilder& DescLayoutBuilder::add_img(
        VkShaderStageFlags stage_flags, uint32_t count
    ) {
        return this->new_binding()
            .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            .set_count(count)
            .set_stage(stage_flags)
            .finish_binding();
    }

    DescLayoutBuilder& DescLayoutBuilder::add_img_tesc(uint32_t cnt) {
        return this->add_img(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, cnt);
    }

    DescLayoutBuilder& DescLayoutBuilder::add_img_tese(uint32_t cnt) {
        return this->add_img(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, cnt);
    }

    DescLayoutBuilder& DescLayoutBuilder::add_input_att(
        VkShaderStageFlags stage_flags, uint32_t count
    ) {
        return this->new_binding()
            .set_type(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            .set_count(count)
            .set_stage(stage_flags)
            .finish_binding();
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
            MIRINAE_ABORT("Failed to create descriptor set layout: {}", name_);
        }

        return handle;
    }

}  // namespace mirinae


// DesclayoutManager
namespace mirinae {

    DesclayoutManager::DesclayoutManager(VulkanDevice& device)
        : device_(device) {}

    DesclayoutManager::~DesclayoutManager() {
        for (auto& item : data_) item.destroy(device_.logi_device());
        data_.clear();
    }

    VkDescriptorSetLayout DesclayoutManager::add(
        const DescLayoutBuilder& builder, VkDevice logi_device
    ) {
        return data_
            .emplace_back(
                builder.name(), builder.size_info(), builder.build(logi_device)
            )
            .layout();
    }

    const DescLayout& DesclayoutManager::get(const std::string& name) const {
        for (auto& item : data_) {
            if (item.name() == name)
                return item;
        }

        MIRINAE_ABORT("Failed to find descriptor set layout: {}", name);
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

    DescWriteInfoBuilder& DescWriteInfoBuilder::add_img_sampler_general(
        VkImageView image_view, VkSampler sampler
    ) {
        auto& image_info = image_info_.emplace_back();
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
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

    DescWriteInfoBuilder& DescWriteInfoBuilder::add_storage_img(
        VkImageView image_view
    ) {
        auto& image_info = image_info_.emplace_back();
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_info.imageView = image_view;
        image_info.sampler = VK_NULL_HANDLE;

        auto& write = data_.emplace_back();
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descset_;
        write.dstBinding = binding_index_++;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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

    void DescPool::init(
        const uint32_t max_sets,
        const uint32_t pool_size_count,
        const VkDescriptorPoolSize* pool_sizes,
        const VkDevice logi_device
    ) {
        this->destroy(logi_device);

        VkDescriptorPoolCreateInfo cinfo{};
        cinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        cinfo.poolSizeCount = pool_size_count;
        cinfo.pPoolSizes = pool_sizes;
        cinfo.maxSets = max_sets;

        VK_CHECK(vkCreateDescriptorPool(logi_device, &cinfo, NULL, &handle_));
    }

    void DescPool::init(
        const uint32_t max_sets,
        const DescSizeInfo& size_info,
        const VkDevice logi_device
    ) {
        size_info_ = size_info;
        size_info_.multiply_counts(max_sets);
        const auto pool_sizes = size_info_.create_arr();

        return this->init(
            max_sets,
            static_cast<uint32_t>(pool_sizes.size()),
            pool_sizes.data(),
            logi_device
        );
    }

    // For rest of methods too
    void DescPool::destroy(VkDevice logi_device) {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSet DescPool::alloc(
        VkDescriptorSetLayout desclayout, VkDevice logi_device
    ) {
        std::vector<VkDescriptorSetLayout> layouts(1, desclayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = handle_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &desclayout;

        VkDescriptorSet output;
        VK_CHECK(vkAllocateDescriptorSets(logi_device, &allocInfo, &output));

        return output;
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
