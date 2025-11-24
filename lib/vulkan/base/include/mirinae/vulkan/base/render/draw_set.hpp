#pragma once

#include <entt/fwd.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/ocean.hpp"
#include "mirinae/vulkan/base/render/renderee.hpp"
#include "mirinae/vulkan/base/renderee/ren_actor_skinned.hpp"


namespace mirinae {

    class DrawSheet {

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


    class DrawSetStatic {

    public:
        struct StaticActor {
            const mirinae::RenderUnit* unit_ = nullptr;
            const mirinae::RenderActor* actor_ = nullptr;
            glm::dmat4 model_mat_{ 1 };
        };

        struct SkinnedActor {
            const RenderUnitSkinned* unit_ = nullptr;
            const RenderActorSkinned* actor_ = nullptr;
            glm::dmat4 model_mat_{ 1 };
            size_t runit_idx_ = 0;
        };

        void fetch(const entt::registry& reg);
        void clear();

        const std::vector<StaticActor>& opa() const { return opa_; }
        const std::vector<StaticActor>& trs() const { return trs_; }
        const std::vector<SkinnedActor>& skin_opa() const { return skin_opa_; }
        const std::vector<SkinnedActor>& skin_trs() const { return skin_trs_; }

    private:
        std::vector<StaticActor> opa_;
        std::vector<StaticActor> trs_;
        std::vector<SkinnedActor> skin_opa_;
        std::vector<SkinnedActor> skin_trs_;
    };


    class DrawSetSkinned {

    public:
        struct SkinnedActor {
            const mirinae::RenderUnitSkinned* unit_ = nullptr;
            const mirinae::RenderActorSkinned* actor_ = nullptr;
            glm::dmat4 model_mat_{ 1 };
        };

        void fetch(const entt::registry& reg);
        void clear();

        const std::vector<SkinnedActor>& opa() const { return opa_; }
        const std::vector<SkinnedActor>& trs() const { return trs_; }

    private:
        std::vector<SkinnedActor> opa_;
        std::vector<SkinnedActor> trs_;
    };

}  // namespace mirinae
