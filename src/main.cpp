#include "mirinae/engine.hpp"


int main() {
    mirinae::Engine engine;

    while (engine.is_ongoing())
        engine.do_frame();

    return 0;
}
