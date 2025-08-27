#include "mirinae/vulkan_pch.h"

#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/renderee/ren_actor_skinned.hpp"


// RenderActorSkinned
namespace mirinae {

    struct RenderActorSkinned::FrameData {
        Buffer ubuf_;
        VkDescriptorSet descset_;
    };


    RenderActorSkinned::RenderActorSkinned(VulkanDevice& vulkan_device)
        : device_(vulkan_device) {}

    RenderActorSkinned::~RenderActorSkinned() { this->destroy(); }

    void RenderActorSkinned::init(
        uint32_t max_flight_count, DesclayoutManager& desclayouts
    ) {
        auto& desclayout = desclayouts.get("gbuf:actor_skinned");
        desc_pool_.init(
            max_flight_count, desclayout.size_info(), device_.logi_device()
        );
        const auto descsets = desc_pool_.alloc(
            max_flight_count, desclayout.layout(), device_.logi_device()
        );

        BufferCreateInfo ubuf_cinfo;
        ubuf_cinfo.preset_ubuf(sizeof(U_GbufActorSkinned));

        DescWriteInfoBuilder builder;
        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& fd = frame_data_.emplace_back();
            fd.ubuf_.init(ubuf_cinfo, device_.mem_alloc());
            fd.descset_ = descsets.at(i);

            builder.set_descset(fd.descset_).add_ubuf(fd.ubuf_);
        }
        builder.apply_all(device_.logi_device());
    }

    void RenderActorSkinned::destroy() {
        frame_data_.clear();
        desc_pool_.destroy(device_.logi_device());
    }

    void RenderActorSkinned::update_ubuf(
        uint32_t index, const U_GbufActorSkinned& data
    ) {
        auto& ubuf = frame_data_.at(index).ubuf_;
        ubuf.set_data(&data, sizeof(U_GbufActorSkinned));
    }

    VkDescriptorSet RenderActorSkinned::get_desc_set(size_t index) const {
        return frame_data_.at(index).descset_;
    }

}  // namespace mirinae
