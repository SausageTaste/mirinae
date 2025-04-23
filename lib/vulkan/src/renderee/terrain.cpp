#include "mirinae/vulkan_pch.h"

#include "mirinae/renderee/terrain.hpp"


// RenUnitTerrain
namespace mirinae {

    RenUnitTerrain::RenUnitTerrain(
        const cpnt::Terrain& src_terr,
        ITextureManager& tex,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    )
        : device_(device) {
        height_map_ = tex.block_for_tex(src_terr.height_map_path_, false);
        albedo_map_ = tex.block_for_tex(src_terr.albedo_map_path_, true);

        if (!height_map_ || !albedo_map_) {
            SPDLOG_ERROR("Failed to load terrain texture.");
            return;
        }

        auto& layout = desclayouts.get("gbuf_terrain:main");
        desc_pool_.init(3, layout.size_info(), device.logi_device());
        desc_set_ = desc_pool_.alloc(layout.layout(), device.logi_device());

        auto& sam = device.samplers();
        mirinae::DescWriteInfoBuilder{}
            .set_descset(desc_set_)
            .add_img_sampler(height_map_->image_view(), sam.get_heightmap())
            .add_img_sampler(albedo_map_->image_view(), sam.get_linear())
            .apply_all(device.logi_device());
    }

    RenUnitTerrain::~RenUnitTerrain() {
        desc_pool_.destroy(device_.logi_device());
    }

    bool RenUnitTerrain::is_ready() const {
        if (!height_map_)
            return false;
        if (!albedo_map_)
            return false;

        return true;
    }

    const dal::IImage* RenUnitTerrain::height_map() const {
        if (height_map_)
            return height_map_->img_data();

        return nullptr;
    }

    VkDescriptorSet RenUnitTerrain::desc_set() const { return desc_set_; }

    VkExtent2D RenUnitTerrain::height_map_size() const {
        return height_map_->extent();
    }

}  // namespace mirinae
