#pragma once

#include "mirinae/cpnt/terrain.hpp"

#include "render/texture.hpp"
#include "render/uniform.hpp"


namespace mirinae {

    class RenUnitTerrain : public ITerrainRenUnit {

    public:
        struct Vertex {
            glm::vec3 pos_;
            glm::vec2 texco_;
        };

        RenUnitTerrain(
            const cpnt::Terrain& src_terr,
            ITextureManager& tex,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        );

        ~RenUnitTerrain() override;

        bool is_ready() const;

        const dal::IImage* height_map() const override;
        VkDescriptorSet desc_set() const;
        VkExtent2D height_map_size() const;

        void draw_indexed(VkCommandBuffer cmdbuf) const;

    private:
        VulkanDevice& device_;
        std::shared_ptr<ITexture> height_map_;
        std::shared_ptr<ITexture> albedo_map_;
        Buffer vtx_buf_, idx_buf_;
        DescPool desc_pool_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
        uint32_t vtx_count_ = 0;
    };

}  // namespace mirinae
