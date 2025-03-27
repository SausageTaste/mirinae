#pragma once

#include <memory>


namespace mirinae {

    class PhysWorld {

    public:
        PhysWorld();
        ~PhysWorld();

        void do_frame(double dt);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
