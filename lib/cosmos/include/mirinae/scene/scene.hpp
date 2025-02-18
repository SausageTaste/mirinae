#pragma once

#include <filesystem>

#include <entt/entt.hpp>
#include <sung/basic/geometry3d.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/lightweight/script.hpp"


namespace mirinae {

    class Scene {

    public:
        Scene(const sung::SimClock& global_clock, ScriptEngine& script);

        const sung::SimClock& clock() const { return clock_; }

        void do_frame();

        // Ray in world space
        void pick_entt(const sung::LineSegment3& ray);

    public:
        constexpr static uint64_t MAGIC_NUM = 46461236464165;

        entt::registry reg_;
        std::vector<entt::entity> entt_without_model_;
        entt::entity main_camera_ = entt::null;
        const uint64_t magic_num_ = MAGIC_NUM;

    private:
        ScriptEngine& script_;
        sung::SimClock clock_;
    };

}  // namespace mirinae
