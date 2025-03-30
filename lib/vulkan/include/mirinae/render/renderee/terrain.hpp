#pragma once

#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/render/texture.hpp"
#include "mirinae/render/uniform.hpp"


namespace mirinae {

    class RenUnitTerrain : public ITerrainRenUnit {

    public:
        RenUnitTerrain(
            const cpnt::Terrain& src_terr,
            ITextureManager& tex,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        );

        ~RenUnitTerrain() override;

        const dal::IImage* height_map() const override {
            return height_map_->img_data();
        }

        VkDescriptorSet desc_set() const;
        VkExtent2D height_map_size() const;

    private:
        VulkanDevice& device_;
        std::shared_ptr<ITexture> height_map_;
        std::shared_ptr<ITexture> albedo_map_;
        DescPool desc_pool_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };

}  // namespace mirinae
