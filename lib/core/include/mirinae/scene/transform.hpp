#pragma once

#include "mirinae/math/mamath.hpp"
#include "mirinae/platform/osio.hpp"
#include "mirinae/util/input_proc.hpp"


namespace mirinae::cpnt {

    using Transform = TransformQuat<double>;

}


namespace mirinae::syst {

    class NoclipController : public IInputProcessor {

    public:
        bool on_key_event(const key::Event& e) override;
        bool on_mouse_event(const mouse::Event& e) override;

        void apply(cpnt::Transform& transform, const double delta_time);

        IOsIoFunctions* osio_ = nullptr;

    private:
        key::EventAnalyzer keys_;
        glm::dvec2 last_mouse_pos_{ 0, 0 };
        glm::dvec2 last_applied_mouse_pos_{ 0, 0 };
        double move_speed_ = 2;
        bool owning_mouse_ = false;
    };

}  // namespace mirinae::syst
