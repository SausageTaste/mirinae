#pragma once

#include <memory>


namespace mirinae {

    class IEngine {

    public:
        virtual void do_frame() = 0;
        virtual bool is_ongoing() = 0;

    };


    std::unique_ptr<IEngine> create_engine();

}
