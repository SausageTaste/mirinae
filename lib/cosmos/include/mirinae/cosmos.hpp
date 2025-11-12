#pragma once

#include <sung/basic/angle.hpp>

#include "mirinae/scene/phys_world.hpp"
#include "mirinae/scene/scene.hpp"
#include "mirinae/system/imgui.hpp"


namespace mirinae {

    class InputActionMapper;
    class TaskGraph;


    struct ICamController {
        virtual ~ICamController() = default;
        virtual void set_target(entt::entity e) = 0;
        virtual void set_camera(entt::entity e) = 0;

        std::string anim_idle_;
        std::string anim_walk_;
        std::string anim_run_;
        std::string anim_sprint_;
        sung::TAngle<double> player_model_heading_;
    };


    class CosmosSimulator {

    public:
        CosmosSimulator(ScriptEngine& script);

        void register_tasks(
            TaskGraph& tasks, mirinae::InputActionMapper& action_map
        );

        void tick_clock();

        auto& scene() { return scene_; }
        auto& reg() { return *scene_.reg_; }
        auto& reg() const { return scene_.reg_; }
        auto& phys_world() { return phys_world_; }
        auto& clock() const { return clock_; }
        auto& cam_ctrl() { return *cam_ctrl_; }

        std::vector<std::shared_ptr<imgui::Widget>> imgui_;

    private:
        Scene scene_;
        PhysWorld phys_world_;
        sung::SimClock clock_;
        std::shared_ptr<ICamController> cam_ctrl_;
    };

    using HCosmos = std::shared_ptr<CosmosSimulator>;

}  // namespace mirinae
