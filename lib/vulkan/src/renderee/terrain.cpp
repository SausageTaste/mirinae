#include "mirinae/render/renderee/terrain.hpp"


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

    VkDescriptorSet RenUnitTerrain::desc_set() const { return desc_set_; }

    VkExtent2D RenUnitTerrain::height_map_size() const {
        return height_map_->extent();
    }

}  // namespace mirinae
