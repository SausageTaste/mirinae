#pragma once

#include <sung/general/time.hpp>


namespace mirinae {

    struct FrameTime {
        double tp_;     // Simulation-time time point in seconds
        double dt_;     // Simulation-time delta time in seconds
        double rt_tp_;  // Real-time time point in seconds
        double rt_dt_;  // Real-time delta time in seconds
    };


    class GlobalClock {

    public:
        FrameTime update() {
            FrameTime output;

            output.rt_tp_ = rt_clock_.elapsed();
            output.rt_dt_ = rt_timer_.check_get_elapsed();
            output.dt_ = output.rt_dt_ * time_scale_;
            sim_time_ += output.dt_;
            output.tp_ = sim_time_;

            return output;
        }

    private:
        sung::MonotonicClock rt_clock_;
        sung::MonotonicClock rt_timer_;
        double sim_time_ = 0;
        double time_scale_ = 1;
    };

}  // namespace mirinae
