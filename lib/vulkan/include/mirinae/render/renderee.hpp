#pragma once

#include <vector>

#include <daltools/scene/struct.h>

#include "mirinae/lightweight/skin_anim.hpp"
#include "mirinae/render/texture.hpp"
#include "mirinae/render/uniform.hpp"
#include "mirinae/render/vkcomposition.hpp"


namespace mirinae {

    class RenderUnit {

    public:
        void init(
            uint32_t max_flight_count,
            const VerticesStaticPair& vertices,
            const U_GbufModel& ubuf_data,
            VkImageView albedo_map,
            VkImageView normal_map,
            VkImageView orm_map,
            CommandPool& cmd_pool,
            DesclayoutManager& desclayouts,
            VulkanDevice& vulkan_device
        );
        void destroy(VulkanMemoryAllocator mem_alloc, VkDevice logi_device);

        VkDescriptorSet get_desc_set(size_t index);
        void record_bind_vert_buf(VkCommandBuffer cmdbuf);
        uint32_t vertex_count() const;

    private:
        DescPool desc_pool_;
        VertexIndexPair vert_index_pair_;
        Buffer uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
    };


    class RenderUnitSkinned {

    public:
        void init(
            uint32_t max_flight_count,
            const VerticesSkinnedPair& vertices,
            const U_GbufModel& ubuf_data,
            VkImageView albedo_map,
            VkImageView normal_map,
            VkImageView orm_map,
            CommandPool& cmd_pool,
            DesclayoutManager& desclayouts,
            VulkanDevice& vulkan_device
        );
        void destroy(VulkanMemoryAllocator mem_alloc, VkDevice logi_device);

        VkDescriptorSet get_desc_set(size_t index);
        void record_bind_vert_buf(VkCommandBuffer cmdbuf);
        uint32_t vertex_count() const;

    private:
        DescPool desc_pool_;
        VertexIndexPair vert_index_pair_;
        Buffer uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
    };


    class OverlayRenderUnit {

    public:
        OverlayRenderUnit(VulkanDevice& device);
        ~OverlayRenderUnit();

        void init(
            uint32_t max_flight_count,
            VkImageView color_view,
            VkImageView mask_view,
            VkSampler sampler,
            DesclayoutManager& desclayouts,
            TextureManager& tex_man
        );
        void destroy();

        void udpate_ubuf(uint32_t index);
        size_t ubuf_count() const { return uniform_buf_.size(); }
        VkDescriptorSet get_desc_set(size_t index);

        U_OverlayMain ubuf_data_;
        U_OverlayPushConst push_const_;

    private:
        VulkanDevice& device_;
        DescPool desc_pool_;
        std::vector<Buffer> uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
    };


    class RenderModel {

    public:
        RenderModel(VulkanDevice& vulkan_device) : device_(vulkan_device) {}
        ~RenderModel();

    public:
        std::vector<RenderUnit> render_units_;
        std::vector<RenderUnit> render_units_alpha_;
        VulkanDevice& device_;
    };


    class RenderModelSkinned {

    public:
        RenderModelSkinned(VulkanDevice& vulkan_device)
            : skel_anim_(std::make_shared<SkelAnimPair>())
            , device_(vulkan_device) {}

        ~RenderModelSkinned();

    public:
        std::vector<RenderUnitSkinned> runits_;
        std::vector<RenderUnitSkinned> runits_alpha_;
        HSkelAnim skel_anim_;
        VulkanDevice& device_;
    };


    class ModelManager {

    public:
        ModelManager(VulkanDevice& device);
        ~ModelManager();

        std::shared_ptr<RenderModel> request_static(
            const mirinae::respath_t& res_id,
            DesclayoutManager& desclayouts,
            TextureManager& tex_man
        );

        std::shared_ptr<RenderModelSkinned> request_skinned(
            const mirinae::respath_t& res_id,
            DesclayoutManager& desclayouts,
            TextureManager& tex_man
        );

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> pimpl_;
    };


    class RenderActor {

    public:
        RenderActor(VulkanDevice& vulkan_device) : device_(vulkan_device) {}
        ~RenderActor() { this->destroy(); }

        void init(uint32_t max_flight_count, DesclayoutManager& desclayouts);
        void destroy();

        void udpate_ubuf(
            uint32_t index,
            const U_GbufActor& data,
            VulkanMemoryAllocator mem_alloc
        );
        VkDescriptorSet get_desc_set(size_t index);

    private:
        DescPool desc_pool_;
        std::vector<Buffer> uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
        VulkanDevice& device_;
    };


    class RenderActorSkinned {

    public:
        RenderActorSkinned(VulkanDevice& vulkan_device)
            : device_(vulkan_device) {}
        ~RenderActorSkinned() { this->destroy(); }

        void init(uint32_t max_flight_count, DesclayoutManager& desclayouts);
        void destroy();

        void udpate_ubuf(
            uint32_t index,
            const U_GbufActorSkinned& data,
            VulkanMemoryAllocator mem_alloc
        );
        VkDescriptorSet get_desc_set(size_t index);

    private:
        DescPool desc_pool_;
        std::vector<Buffer> uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
        VulkanDevice& device_;
    };


    namespace cpnt {

        struct StaticActorVk {
            std::shared_ptr<mirinae::RenderModel> model_;
            std::shared_ptr<mirinae::RenderActor> actor_;
        };


        struct SkinnedActorVk {
            std::shared_ptr<mirinae::RenderModelSkinned> model_;
            std::shared_ptr<mirinae::RenderActorSkinned> actor_;
        };

    }  // namespace cpnt

}  // namespace mirinae
