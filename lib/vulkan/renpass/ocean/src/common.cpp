#include "common.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/vulkan/base/renderpass/builder.hpp"


namespace mirinae {

    const cpnt::Ocean* find_ocean_cpnt(const entt::registry& reg) {
        for (auto e : reg.view<cpnt::Ocean>()) {
            // Only one ocean is allowed
            return &reg.get<cpnt::Ocean>(e);
        }
        return nullptr;
    }

}  // namespace mirinae
