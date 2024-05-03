#pragma once

#include "mirinae/platform/osio.hpp"
#include "mirinae/util/mamath.hpp"
#include "mirinae/util/uinput.hpp"


namespace mirinae::cpnt {

    using Transform = TransformQuat<double>;

}


namespace mirinae::syst {

    class NoclipController {

    public:
        bool on_key_event(const key::Event& e);
        bool on_mouse_event(const mouse::Event& e, IOsIoFunctions& osio);

        void apply(cpnt::Transform& transform, double delta_time);

    private:
        key::EventAnalyzer key_states_;
        glm::dvec2 last_mouse_pos_{ 0, 0 };
        glm::dvec2 last_applied_mouse_pos_{ 0, 0 };
        double move_speed_ = 2;
        bool owning_mouse_ = false;
    };

}  // namespace mirinae::syst
