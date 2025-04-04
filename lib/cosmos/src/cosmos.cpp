#include "mirinae/cosmos.hpp"


namespace mirinae {

    CosmosSimulator::CosmosSimulator(ScriptEngine& script)
        : scene_(clock_, script) {}

    void CosmosSimulator::tick_clock() { clock_.tick(); }

    void CosmosSimulator::do_frame() {
        phys_world_.pre_sync(scene_.clock().dt(), *scene_.reg_);
        phys_world_.do_frame(scene_.clock().dt());
        phys_world_.post_sync(scene_.clock().dt(), *scene_.reg_);
    }

}  // namespace mirinae
