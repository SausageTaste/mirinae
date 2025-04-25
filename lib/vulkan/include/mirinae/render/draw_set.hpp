#pragma once

#include <entt/fwd.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/ocean.hpp"
#include "mirinae/render/renderee.hpp"


namespace mirinae {

    struct DrawSheet {

    public:
        struct StaticRenderPairs {
            struct Actor {
                mirinae::RenderActor* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderUnit* unit_;
            std::vector<Actor> actors_;
        };

        struct SkinnedRenderPairs {
            struct Actor {
                mirinae::RenderActorSkinned* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderUnitSkinned* unit_;
            std::vector<Actor> actors_;
        };

    public:
        void build(entt::registry& reg);

    private:
        StaticRenderPairs& get_static(mirinae::RenderUnit& unit);
        StaticRenderPairs& get_static_trs(mirinae::RenderUnit& unit);
        SkinnedRenderPairs& get_skinned(mirinae::RenderUnitSkinned& unit);
        SkinnedRenderPairs& get_skinned_trs(mirinae::RenderUnitSkinned& unit);

    public:
        std::vector<StaticRenderPairs> static_;
        std::vector<StaticRenderPairs> static_trs_;
        std::vector<SkinnedRenderPairs> skinned_;
        std::vector<SkinnedRenderPairs> skinned_trs_;
        cpnt::Ocean* ocean_ = nullptr;
        cpnt::AtmosphereSimple* atmosphere_ = nullptr;
    };

}  // namespace mirinae
