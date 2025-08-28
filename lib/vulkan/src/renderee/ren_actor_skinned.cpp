#include "mirinae/vulkan_pch.h"

#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/renderee/ren_actor_skinned.hpp"


namespace {

    struct RenderUnit : public mirinae::RenderActorSkinned::RenUnit {};

}  // namespace


// RenderActorSkinned
namespace mirinae {

    struct RenderActorSkinned::FrameData {
        std::vector<RenderUnit> runits_;
        Buffer ubuf_;
        VkDescriptorSet descset_;
    };


    RenderActorSkinned::RenderActorSkinned(VulkanDevice& vulkan_device)
        : device_(vulkan_device) {}

    RenderActorSkinned::~RenderActorSkinned() { this->destroy(); }

    void RenderActorSkinned::init(
        const uint32_t max_flight_count,
        const std::vector<RenUnitInfo>& runit_info,
        const DesclayoutManager& desclayouts
    ) {
        const auto desc_count = max_flight_count * runit_info.size();
        auto& desclayout = desclayouts.get("gbuf:actor_skinned");
        auto& desclayout_ren = desclayouts.get("skin_anim:main");

        desc_pool_.init(
            max_flight_count + desc_count,
            desclayout_ren.size_info(),
            device_.logi_device()
        );
        auto descsets = desc_pool_.alloc(
            max_flight_count, desclayout.layout(), device_.logi_device()
        );
        auto descsets_ren = desc_pool_.alloc(
            desc_count, desclayout_ren.layout(), device_.logi_device()
        );

        BufferCreateInfo ubuf_cinfo;
        ubuf_cinfo.preset_ubuf(sizeof(U_GbufActorSkinned));

        BufferCreateInfo vbuf_cinfo;
        vbuf_cinfo.set_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
            .add_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& fd = frame_data_.emplace_back();
            fd.ubuf_.init(ubuf_cinfo, device_.mem_alloc());

            fd.descset_ = descsets.back();
            descsets.pop_back();
            MIRINAE_ASSERT(VK_NULL_HANDLE != fd.descset_);

            {
                DescWriter dw;
                dw.add_buf_info(fd.ubuf_).add_buf_write(fd.descset_, 0);
                dw.apply_all(device_.logi_device());
            }

            for (auto& src_unit : runit_info) {
                auto& dst_unit = fd.runits_.emplace_back();

                vbuf_cinfo.set_size(src_unit.src_vtx_buf_->size());
                dst_unit.vertex_buf_.init(vbuf_cinfo, device_.mem_alloc());

                dst_unit.descset_ = descsets_ren.back();
                descsets_ren.pop_back();
                MIRINAE_ASSERT(VK_NULL_HANDLE != dst_unit.descset_);

                {
                    DescWriter dw;
                    dw.add_buf_info(dst_unit.vertex_buf_)
                        .add_storage_buf_write(dst_unit.descset_, 0);
                    dw.add_buf_info(*src_unit.src_vtx_buf_)
                        .add_storage_buf_write(dst_unit.descset_, 1);
                    dw.add_buf_info(fd.ubuf_).add_buf_write(
                        dst_unit.descset_, 2
                    );
                    dw.apply_all(device_.logi_device());
                }
            }
        }
    }

    void RenderActorSkinned::destroy() {
        frame_data_.clear();
        desc_pool_.destroy(device_.logi_device());
    }

    void RenderActorSkinned::update_ubuf(
        uint32_t index, const U_GbufActorSkinned& data
    ) {
        auto& ubuf = frame_data_.at(index).ubuf_;
        ubuf.set_data(&data, sizeof(U_GbufActorSkinned));
    }

    VkDescriptorSet RenderActorSkinned::get_desc_set(size_t index) const {
        return frame_data_.at(index).descset_;
    }

    const RenderActorSkinned::RenUnit& RenderActorSkinned::get_runit(
        FrameIndex f_idx, size_t unit_idx
    ) const {
        return frame_data_.at(f_idx).runits_.at(unit_idx);
    }

}  // namespace mirinae
