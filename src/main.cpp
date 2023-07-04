#include "mirinae/engine.hpp"


int main() {
    auto engine = mirinae::create_engine();

    while (engine->is_ongoing())
        engine->do_frame();

    return 0;
}
