#pragma once

#include "mirinae/util/mamath.hpp"
#include "mirinae/util/uinput.hpp"



namespace mirinae::cpnt {

    using Transform = TransformQuat;

}


namespace mirinae::syst {

    class NoclipController {

    public:
        bool on_key_event(const key::Event& e);

        void apply(cpnt::Transform& transform, float delta_time) const;

    private:
        key::EventAnalyzer key_states_;

    };

}
