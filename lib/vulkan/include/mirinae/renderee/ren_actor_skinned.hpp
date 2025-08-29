#pragma once

#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/render/uniform.hpp"


namespace mirinae {

    class RenderActorSkinned : public IRenActor {

    public:
        struct RenUnitInfo {
            const Buffer* src_vtx_buf_ = nullptr;
            bool transparent_ = false;
        };

        struct IRenUnit {
            virtual const Buffer& vertex_buf(FrameIndex f_idx) const = 0;
            virtual VkDescriptorSet descset(FrameIndex f_idx) const = 0;
        };

    public:
        RenderActorSkinned(VulkanDevice& vulkan_device);
        ~RenderActorSkinned();

        void init(
            const uint32_t max_flight_count,
            const std::vector<RenUnitInfo>& runit_info,
            const DesclayoutManager& desclayouts
        );
        void destroy();

        void update_ubuf(
            const FrameIndex f_index,
            const U_GbufActor& static_data,
            const U_GbufActorSkinned& skinned_data
        );

        VkDescriptorSet get_descset_static(FrameIndex f_index) const;
        VkDescriptorSet get_desc_set(size_t f_index) const;
        const IRenUnit& get_runit(size_t unit_idx) const;

    private:
        class FrameData;
        class RenUnit;

        DescPool desc_pool_;
        std::vector<FrameData> frame_data_;
        std::vector<RenUnit> runits_;
        VulkanDevice& device_;
    };

}  // namespace mirinae
