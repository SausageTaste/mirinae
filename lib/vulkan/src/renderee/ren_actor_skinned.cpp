#include "mirinae/vulkan_pch.h"

#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/renderee/ren_actor_skinned.hpp"


// RenderActorSkinned
namespace mirinae {

#define CLS RenderActorSkinned

    class CLS::FrameData {

    public:
        Buffer ubuf_static_;   // U_GbufActor
        Buffer ubuf_skinned_;  // U_GbufActorSkinned
        VkDescriptorSet descset_static_;
        VkDescriptorSet descset_skinned_;
    };


    class CLS::RenUnit : public CLS::IRenUnit {

    public:
        const Buffer& vertex_buf(FrameIndex f_idx) const override {
            return frame_data_.at(f_idx.get()).vtx_buf_;
        }

        VkDescriptorSet descset(FrameIndex f_idx) const override {
            return frame_data_.at(f_idx.get()).descset_;
        }

        struct FrameData {
            Buffer vtx_buf_;
            VkDescriptorSet descset_;
        };

        std::vector<FrameData> frame_data_;
    };


    CLS::RenderActorSkinned(VulkanDevice& vulkan_device)
        : device_(vulkan_device) {}

    CLS::~RenderActorSkinned() { this->destroy(); }

    void CLS::init(
        const uint32_t max_flight_count,
        const std::vector<RenUnitInfo>& runit_info,
        const DesclayoutManager& desclayouts
    ) {
        const auto runit_count = static_cast<uint32_t>(runit_info.size());

        auto& desclayout_static = desclayouts.get("gbuf:actor");
        auto& desclayout_skinned = desclayouts.get("gbuf:actor_skinned");
        auto& desclayout_anim = desclayouts.get("skin_anim:main");

        desc_pool_.init(
            (2 * max_flight_count) + (runit_count * max_flight_count),
            desclayout_anim.size_info(),
            device_.logi_device()
        );
        auto descsets_static = desc_pool_.alloc(
            max_flight_count, desclayout_static.layout(), device_.logi_device()
        );
        auto descsets_skinned = desc_pool_.alloc(
            max_flight_count, desclayout_skinned.layout(), device_.logi_device()
        );
        auto descsets_anim = desc_pool_.alloc(
            runit_count * max_flight_count,
            desclayout_anim.layout(),
            device_.logi_device()
        );

        BufferCreateInfo ubuf_static_cinfo;
        ubuf_static_cinfo.preset_ubuf(sizeof(U_GbufActor));

        BufferCreateInfo ubuf_skinned_cinfo;
        ubuf_skinned_cinfo.preset_ubuf(sizeof(U_GbufActorSkinned));

        BufferCreateInfo vbuf_cinfo;
        vbuf_cinfo.set_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
            .add_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        DescWriter dw;

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& fd = frame_data_.emplace_back();
            fd.ubuf_static_.init(ubuf_static_cinfo, device_.mem_alloc());
            fd.ubuf_skinned_.init(ubuf_skinned_cinfo, device_.mem_alloc());

            fd.descset_static_ = descsets_static.back();
            descsets_static.pop_back();

            fd.descset_skinned_ = descsets_skinned.back();
            descsets_skinned.pop_back();

            MIRINAE_ASSERT(VK_NULL_HANDLE != fd.descset_static_);
            MIRINAE_ASSERT(VK_NULL_HANDLE != fd.descset_skinned_);

            dw.add_buf_info(fd.ubuf_static_)
                .add_buf_write(fd.descset_static_, 0);
            dw.add_buf_info(fd.ubuf_skinned_)
                .add_buf_write(fd.descset_skinned_, 0);
        }

        for (auto& src_unit : runit_info) {
            auto& dst_unit = (src_unit.transparent_)
                                 ? runits_trs_.emplace_back()
                                 : runits_.emplace_back();

            for (uint32_t i = 0; i < max_flight_count; ++i) {
                const auto& fd = frame_data_.at(i);
                auto& dst_fd = dst_unit.frame_data_.emplace_back();

                vbuf_cinfo.set_size(src_unit.src_vtx_buf_->size());
                dst_fd.vtx_buf_.init(vbuf_cinfo, device_.mem_alloc());

                dst_fd.descset_ = descsets_anim.back();
                descsets_anim.pop_back();
                MIRINAE_ASSERT(VK_NULL_HANDLE != dst_fd.descset_);

                dw.add_buf_info(dst_fd.vtx_buf_)
                    .add_storage_buf_write(dst_fd.descset_, 0);
                dw.add_buf_info(*src_unit.src_vtx_buf_)
                    .add_storage_buf_write(dst_fd.descset_, 1);
                dw.add_buf_info(fd.ubuf_skinned_)
                    .add_buf_write(dst_fd.descset_, 2);
            }
        }

        dw.apply_all(device_.logi_device());
    }

    void CLS::destroy() {
        frame_data_.clear();
        desc_pool_.destroy(device_.logi_device());
    }

    void CLS::update_ubuf(
        const FrameIndex f_index,
        const U_GbufActor& static_data,
        const U_GbufActorSkinned& skinned_data
    ) {
        auto& fd = frame_data_.at(f_index.get());
        fd.ubuf_static_.set_data(&static_data, sizeof(U_GbufActor));
        fd.ubuf_skinned_.set_data(&skinned_data, sizeof(U_GbufActorSkinned));
    }

    VkDescriptorSet CLS::get_descset_static(FrameIndex f_index) const {
        return frame_data_.at(f_index.get()).descset_static_;
    }

    const CLS::IRenUnit& CLS::get_runit(size_t unit_idx) const {
        return runits_.at(unit_idx);
    }

    const CLS::IRenUnit& CLS::get_runit_trs(size_t unit_idx) const {
        return runits_trs_.at(unit_idx);
    }

#undef CLS

}  // namespace mirinae
