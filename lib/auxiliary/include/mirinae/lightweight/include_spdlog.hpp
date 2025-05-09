#pragma once

#ifdef SPDLOG_ACTIVE_LEVEL
    #undef SPDLOG_ACTIVE_LEVEL
#endif

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#define MIRINAE_ABORT(...)                         \
    do {                                           \
        const auto msg = fmt::format(__VA_ARGS__); \
        SPDLOG_CRITICAL("{}", msg);                \
        std::abort();                              \
    } while (0)

#define MIRINAE_ASSERT(cond)                        \
    do {                                            \
        if (!(cond)) {                              \
            MIRINAE_ABORT("Assert failed: " #cond); \
        }                                           \
    } while (0)

#define MIRINAE_ASSERTM(cond, ...)      \
    do {                                \
        if (!(cond)) {                  \
            MIRINAE_ABORT(__VA_ARGS__); \
        }                               \
    } while (0)

#define MIRINAE_VERIFY(cond)                            \
    do {                                                \
        if (!(cond)) {                                  \
            SPDLOG_WARN("Verification failed: " #cond); \
        }                                               \
    } while (0)

#define MIRINAE_VERIFYM(cond, ...)    \
    do {                              \
        if (!(cond)) {                \
            SPDLOG_WARN(__VA_ARGS__); \
        }                             \
    } while (0)
