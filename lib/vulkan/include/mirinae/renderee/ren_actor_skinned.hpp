#pragma once

#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/render/uniform.hpp"


namespace mirinae {

    class RenderActorSkinned : public IRenActor {

    public:
        RenderActorSkinned(VulkanDevice& vulkan_device);
        ~RenderActorSkinned();

        void init(uint32_t max_flight_count, DesclayoutManager& desclayouts);
        void destroy();

        void update_ubuf(uint32_t index, const U_GbufActorSkinned& data);
        VkDescriptorSet get_desc_set(size_t index) const;

    private:
        struct FrameData;

        DescPool desc_pool_;
        std::vector<FrameData> frame_data_;
        VulkanDevice& device_;
    };

}  // namespace mirinae
