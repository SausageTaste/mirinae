#pragma once

#include <memory>


namespace mirinae {

    class PhysWorld {

    public:
        PhysWorld();
        ~PhysWorld();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
