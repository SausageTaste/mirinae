#pragma once

#ifdef SPDLOG_ACTIVE_LEVEL
    #undef SPDLOG_ACTIVE_LEVEL
#endif

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#define MIRINAE_ABORT(...)                         \
    {                                              \
        const auto msg = fmt::format(__VA_ARGS__); \
        SPDLOG_CRITICAL(msg);                      \
        std::abort();                              \
    }                                              \
    while (0)

#define MIRINAE_ASSERT(cond)                    \
    if (!(cond)) {                              \
        MIRINAE_ABORT("Assert failed: " #cond); \
    }                                           \
    while (0)

#define MIRINAE_ASSERTM(cond, ...)  \
    if (!(cond)) {                  \
        MIRINAE_ABORT(__VA_ARGS__); \
    }                               \
    while (0)
