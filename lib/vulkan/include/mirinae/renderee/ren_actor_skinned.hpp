#pragma once

#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/render/uniform.hpp"


namespace mirinae {

    class RenderActorSkinned : public IRenActor {

    public:
        struct RenUnitInfo {
            const mirinae::Buffer* src_vtx_buf_=nullptr;
            bool transparent_ = false;
        };

        struct RenUnit {
            mirinae::Buffer vertex_buf_;
            VkDescriptorSet descset_;
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

        void update_ubuf(uint32_t index, const U_GbufActorSkinned& data);
        VkDescriptorSet get_desc_set(size_t index) const;
        const RenUnit& get_runit(FrameIndex f_idx, size_t unit_idx) const;

    private:
        struct FrameData;

        DescPool desc_pool_;
        std::vector<FrameData> frame_data_;
        VulkanDevice& device_;
    };

}  // namespace mirinae
