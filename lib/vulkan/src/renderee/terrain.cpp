#include "vulkan_pch.h"

#include <sung/basic/mesh_builder.hpp>

#include "render/cmdbuf.hpp"
#include "render/mem_cinfo.hpp"
#include "renderee/terrain.hpp"


namespace {

    class MeshBuilder {

    public:
        using Vertex = mirinae::RenUnitTerrain::Vertex;

        void append(const Vertex& vtx) {
            idx_.push_back(static_cast<uint32_t>(vtx_.size()));
            vtx_.push_back(vtx);
        }

        void add(const Vertex& vtx) {
            if (auto idx = this->find_identical(vtx))
                idx_.push_back(*idx);
            else
                this->append(vtx);
        }

        auto& vtx() const { return vtx_; }
        auto& idx() const { return idx_; }

    private:
        std::optional<uint32_t> find_identical(const Vertex& vtx) const {
            for (size_t i = 0; i < vtx_.size(); ++i) {
                const auto idx = vtx_.size() - 1 - i;
                auto& criterion = vtx_[idx];

                if (criterion.pos_ != vtx.pos_)
                    continue;
                if (criterion.texco_ != vtx.texco_)
                    continue;

                MIRINAE_ASSERT(idx < std::numeric_limits<uint32_t>::max());
                return static_cast<uint32_t>(idx);
            }

            return std::nullopt;
        }

        std::vector<Vertex> vtx_;
        std::vector<uint32_t> idx_;
    };

}  // namespace


// RenUnitTerrain
namespace mirinae {

    RenUnitTerrain::RenUnitTerrain(
        const cpnt::Terrain& src_terr,
        ITextureManager& tex,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    )
        : device_(device) {
        // Load textures
        {
            height_map_ = tex.block_for_tex(src_terr.height_map_path_, false);
            albedo_map_ = tex.block_for_tex(src_terr.albedo_map_path_, true);

            if (!height_map_ || !albedo_map_) {
                SPDLOG_ERROR("Failed to load terrain texture.");
                return;
            }
        }

        // Create desc set
        {
            auto& layout = desclayouts.get("gbuf_terrain:main");
            desc_pool_.init(3, layout.size_info(), device.logi_device());
            desc_set_ = desc_pool_.alloc(layout.layout(), device.logi_device());
        }

        // Write desc set
        {
            auto& sam = device.samplers();
            mirinae::DescWriteInfoBuilder{}
                .set_descset(desc_set_)
                .add_img_sampler(height_map_->image_view(), sam.get_heightmap())
                .add_img_sampler(albedo_map_->image_view(), sam.get_linear())
                .apply_all(device.logi_device());
        }

        MeshBuilder mesh_buil;

        // Create vertex buffer
        {
            const glm::dvec2 tile_size{
                src_terr.terrain_width_ / src_terr.tile_count_x_,
                src_terr.terrain_height_ / src_terr.tile_count_y_
            };

            for (int x = 0; x < src_terr.tile_count_x_; ++x) {
                for (int y = 0; y < src_terr.tile_count_y_; ++y) {
                    const glm::dvec2 tile_offset{
                        x * tile_size.x - src_terr.terrain_width_ * 0.5,
                        y * tile_size.y - src_terr.terrain_height_ * 0.5
                    };

                    const auto x0 = tile_offset.x;
                    const auto x1 = x0 + tile_size.x;
                    const auto y0 = tile_offset.y;
                    const auto y1 = y0 + tile_size.y;

                    const auto u0 = static_cast<double>(x) /
                                    src_terr.tile_count_x_;
                    const auto u1 = static_cast<double>(x + 1) /
                                    src_terr.tile_count_x_;
                    const auto v0 = static_cast<double>(y) /
                                    src_terr.tile_count_y_;
                    const auto v1 = static_cast<double>(y + 1) /
                                    src_terr.tile_count_y_;

                    ::MeshBuilder::Vertex v;

                    v.pos_ = glm::dvec3{ x0, 0, y0 };
                    v.texco_ = glm::dvec2{ u0, v0 };
                    mesh_buil.add(v);

                    v.pos_ = glm::dvec3{ x0, 0, y1 };
                    v.texco_ = glm::dvec2{ u0, v1 };
                    mesh_buil.add(v);

                    v.pos_ = glm::dvec3{ x1, 0, y1 };
                    v.texco_ = glm::dvec2{ u1, v1 };
                    mesh_buil.add(v);

                    v.pos_ = glm::dvec3{ x1, 0, y0 };
                    v.texco_ = glm::dvec2{ u1, v0 };
                    mesh_buil.add(v);
                }
            }
        }

        // Vertex buffer
        {
            const auto data_size = sizeof(::MeshBuilder::Vertex) *
                                   mesh_buil.vtx().size();
            mirinae::BufferCreateInfo cinfo;
            cinfo.set_size(data_size)
                .add_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                .add_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
                .add_alloc_flag_host_access_seq_write();
            vtx_buf_.init(cinfo, device.mem_alloc());
            vtx_buf_.set_data(mesh_buil.vtx().data(), data_size);
        }

        // Index buffer
        {
            const auto data_size = sizeof(uint32_t) * mesh_buil.idx().size();
            mirinae::BufferCreateInfo cinfo;
            cinfo.set_size(data_size)
                .add_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                .add_usage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
                .add_alloc_flag_host_access_seq_write();
            idx_buf_.init(cinfo, device.mem_alloc());
            idx_buf_.set_data(mesh_buil.idx().data(), data_size);
        }

        vtx_count_ = static_cast<uint32_t>(mesh_buil.idx().size());
    }

    RenUnitTerrain::~RenUnitTerrain() {
        desc_pool_.destroy(device_.logi_device());
        vtx_buf_.destroy();
        idx_buf_.destroy();
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

    void RenUnitTerrain::draw_indexed(VkCommandBuffer cmdbuf) const {
        BindVertBufInfo<1>{}.set_at<0>(vtx_buf_).record(cmdbuf);
        bind_idx_buf(cmdbuf, idx_buf_);
        vkCmdDrawIndexed(cmdbuf, vtx_count_, 1, 0, 0, 0);
    }

}  // namespace mirinae
