#pragma once

#include <vector>

#include <daltools/scene/struct.h>

#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/lightweight/skin_anim.hpp"
#include "mirinae/render/texture.hpp"
#include "mirinae/render/uniform.hpp"
#include "mirinae/render/vkcomposition.hpp"


namespace mirinae {

    class RenderUnit {

    public:
        void init(
            const std::string& name,
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

        const std::string& name() const { return name_; }
        VkDescriptorSet get_desc_set(size_t index) const;
        void record_bind_vert_buf(VkCommandBuffer cmdbuf) const;
        uint32_t vertex_count() const;

        auto& raw_data() const { return raw_data_; }

    private:
        std::string name_;
        VerticesStaticPair raw_data_;
        DescPool desc_pool_;
        VertexIndexPair vert_index_pair_;
        Buffer uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
    };


    class RenderUnitSkinned {

    public:
        void init(
            const std::string& name,
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

        const std::string& name() const { return name_; }
        VkDescriptorSet get_desc_set(size_t index) const;
        void record_bind_vert_buf(VkCommandBuffer cmdbuf) const;
        uint32_t vertex_count() const;

    private:
        std::string name_;
        DescPool desc_pool_;
        VertexIndexPair vert_index_pair_;
        Buffer uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
    };


    class OverlayRenderUnit {

    public:
        OverlayRenderUnit(VulkanDevice& device);
        ~OverlayRenderUnit();

        OverlayRenderUnit(OverlayRenderUnit&&) noexcept;
        OverlayRenderUnit& operator=(OverlayRenderUnit&&) noexcept;

        void init(
            uint32_t max_flight_count,
            VkImageView color_view,
            VkImageView mask_view,
            VkSampler sampler,
            DesclayoutManager& desclayouts,
            ITextureManager& tex_man
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


    class RenderModel : public IRenModel {

    public:
        RenderModel(VulkanDevice& vulkan_device);
        ~RenderModel();

        bool is_ready() const override;
        void access_positions(IModelAccessor& acc) const override;

        size_t ren_unit_count() const override;
        std::string_view ren_unit_name(size_t index) const override;

    public:
        std::vector<RenderUnit> render_units_;
        std::vector<RenderUnit> render_units_alpha_;
        VulkanDevice& device_;
    };


    class RenderModelSkinned : public IRenModel {

    public:
        RenderModelSkinned(VulkanDevice& vulkan_device);
        ~RenderModelSkinned();

        bool is_ready() const override;
        void access_positions(IModelAccessor& acc) const override;

        size_t ren_unit_count() const override;
        std::string_view ren_unit_name(size_t index) const override;

    public:
        std::vector<RenderUnitSkinned> runits_;
        std::vector<RenderUnitSkinned> runits_alpha_;
        HSkelAnim skel_anim_;
        VulkanDevice& device_;
    };

    using HRenMdlStatic = std::shared_ptr<RenderModel>;
    using HRenMdlSkinned = std::shared_ptr<RenderModelSkinned>;


    struct IModelManager {
        virtual ~IModelManager() = default;

        virtual dal::ReqResult request_static(const dal::path& res_id) = 0;
        virtual dal::ReqResult request_skinned(const dal::path& res_id) = 0;

        virtual HRenMdlStatic get_static(const dal::path& res_id) = 0;
        virtual HRenMdlSkinned get_skinned(const dal::path& res_id) = 0;
    };

    using HMdlMgr = std::shared_ptr<IModelManager>;
    HMdlMgr create_model_mgr(
        sung::HTaskSche task_sche,
        HTexMgr tex_man,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );


    class RenderActor : public IRenActor {

    public:
        RenderActor(VulkanDevice& vulkan_device);
        ~RenderActor();

        void init(uint32_t max_flight_count, DesclayoutManager& desclayouts);
        void destroy();

        void udpate_ubuf(uint32_t index, const U_GbufActor& data);
        VkDescriptorSet get_desc_set(size_t index) const;

    private:
        DescPool desc_pool_;
        std::vector<Buffer> uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
        VulkanDevice& device_;
    };


    class RenderActorSkinned : public IRenActor {

    public:
        RenderActorSkinned(VulkanDevice& vulkan_device);
        ~RenderActorSkinned();

        void init(uint32_t max_flight_count, DesclayoutManager& desclayouts);
        void destroy();

        void udpate_ubuf(
            uint32_t index,
            const U_GbufActorSkinned& data,
            VulkanMemoryAllocator mem_alloc
        );
        VkDescriptorSet get_desc_set(size_t index) const;

    private:
        DescPool desc_pool_;
        std::vector<Buffer> uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
        VulkanDevice& device_;
    };

}  // namespace mirinae
