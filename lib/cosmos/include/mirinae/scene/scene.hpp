#pragma once

#include <filesystem>

#include <daltools/common/task_sys.hpp>
#include <entt/fwd.hpp>
#include <sung/basic/geometry3d.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/lightweight/script.hpp"


namespace mirinae {

    class TaskGraph;


    class Scene {

    public:
        Scene(const sung::SimClock& global_clock, ScriptEngine& script);

        const sung::SimClock& clock() const { return clock_; }

        void do_frame();

        void register_tasks(TaskGraph& tasks);

        entt::entity find_entt(const std::string& name) const;

        // Ray in world space
        void pick_entt(const sung::LineSegment3& ray);

    public:
        std::shared_ptr<entt::registry> reg_;
        std::vector<entt::entity> entt_without_model_;
        entt::entity main_camera_;
        const uint64_t magic_num_;

    private:
        ScriptEngine& script_;
        sung::SimClock clock_;
    };

}  // namespace mirinae
