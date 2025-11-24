#include "common.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/vulkan/base/renderee/atmos.hpp"


namespace {

    const mirinae::cpnt::AtmosphereEpic* find_atmos_cpnt(
        const entt::registry& reg
    ) {
        auto view = reg.view<mirinae::cpnt::AtmosphereEpic>();
        for (auto e : view) {
            return &view.get<mirinae::cpnt::AtmosphereEpic>(e);
        }
        return nullptr;
    }

    const mirinae::BufferSpan* find_atmos_ubuf(
        const mirinae::cpnt::AtmosphereEpic* atmos_cpnt,
        mirinae::FrameIndex f_idx
    ) {
        if (!atmos_cpnt)
            return nullptr;
        auto ren_unit = atmos_cpnt->ren_unit<mirinae::RenUnitAtmosEpic>();
        if (!ren_unit)
            return nullptr;
        return &ren_unit->ubuf_at(f_idx);
    }

}  // namespace


// IAtmosFrameData
namespace mirinae {

    bool IAtmosFrameData::try_update(
        const entt::registry& reg, const RpCtxtBase& ctxt, VulkanDevice& device
    ) {
        auto atmos_cpnt = find_atmos_cpnt(reg);
        auto ubuf = find_atmos_ubuf(atmos_cpnt, ctxt.f_index_);
        if (!ubuf)
            return false;

        if (ubuf_span_ != *ubuf) {
            ubuf_span_ = *ubuf;
            this->update_descset(device);
        }

        return ubuf_span_.buf_ != VK_NULL_HANDLE;
    }

}  // namespace mirinae
