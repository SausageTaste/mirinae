#pragma once

#include <entt/fwd.hpp>

#include "mirinae/context/base.hpp"
#include "mirinae/cpnt/atmos.hpp"
#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class IAtmosFrameData {

    public:
        virtual ~IAtmosFrameData() = default;
        virtual void update_descset(VulkanDevice& device) = 0;

        bool try_update(
            const entt::registry& reg,
            const RpCtxtBase& ctxt,
            VulkanDevice& device
        );

        BufferSpan ubuf_span_;
    };

}  // namespace mirinae
