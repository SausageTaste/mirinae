#pragma once

#include <memory>

#include <entt/fwd.hpp>

#include "mirinae/lightweight/debug_ren.hpp"


namespace mirinae {

    class PhysWorld {

    public:
        PhysWorld();
        ~PhysWorld();

        void optimize();

        void give_debug_ren(IDebugRen& debug_ren);
        void remove_debug_ren();

        void pre_sync(double dt, entt::registry& reg);
        void do_frame(double dt);
        void post_sync(double dt, entt::registry& reg);

        void give_body(entt::entity entity, entt::registry& reg);
        void give_body_height_field(entt::entity entity, entt::registry& reg);

        void give_body_player(
            double height,
            double radius,
            entt::entity entity,
            entt::registry& reg
        );

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
