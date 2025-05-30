#pragma once

#include <filesystem>

#include <entt/fwd.hpp>

#include "mirinae/cpnt/ocean.hpp"
#include "mirinae/render/vkdevice.hpp"


#define GET_OCEAN_ENTT(ctxt)                        \
    if (!(ctxt).draw_sheet_)                        \
        return;                                     \
    if (!(ctxt).draw_sheet_->ocean_)                \
        return;                                     \
    auto& ocean_entt = *(ctxt).draw_sheet_->ocean_; \
    auto cmdbuf = (ctxt).cmdbuf_;


namespace mirinae {

    constexpr uint32_t CASCADE_COUNT = mirinae::cpnt::OCEAN_CASCADE_COUNT;
    constexpr uint32_t OCEAN_TEX_DIM = 256;
    const uint32_t OCEAN_TEX_DIM_LOG2 = std::log(OCEAN_TEX_DIM) / std::log(2);


    const cpnt::Ocean* find_ocean_cpnt(const entt::registry& reg);

}  // namespace mirinae
