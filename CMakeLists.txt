cmake_minimum_required(VERSION 3.12)
project(mirinae)

include(FetchContent)


set(MIRINAE_ENABLE_TEST OFF CACHE BOOL "Enable testing")
set(MIRINAE_BUILD_APP OFF CACHE BOOL "Build projects in app folder")

FetchContent_Declare(
    sung_fetch_googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG        v1.14.0
)


add_subdirectory(lib)


if (MIRINAE_BUILD_APP)
    add_subdirectory(app)
endif()


if (MIRINAE_ENABLE_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(
        sung_fetch_googletest
    )
    enable_testing()
    add_subdirectory(test)
endif()
