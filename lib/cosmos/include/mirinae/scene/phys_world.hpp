#pragma once

#include <memory>

#include <entt/fwd.hpp>


namespace mirinae {

    class PhysWorld {

    public:
        PhysWorld();
        ~PhysWorld();

        void do_frame(double dt);
        void sync_tforms(entt::registry& reg);

        void give_body(entt::entity entity, entt::registry& reg);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
