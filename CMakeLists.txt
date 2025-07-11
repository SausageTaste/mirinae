cmake_minimum_required(VERSION 3.16)

if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(mirinae
    VERSION 0.1.1
    LANGUAGES CXX
)

set(MIRINAE_ENABLE_TEST OFF CACHE BOOL "Enable testing")
set(MIRINAE_BUILD_APP ON CACHE BOOL "Build projects in app folder")
set(MIRINAE_USE_DALBARAGI_SUBMODULE ON CACHE BOOL "Path to dalbaragi submodule")
set(DAL_USE_SUNGTOOLS_SUBMODULE ON CACHE BOOL "Use sungtools as a submodule")


if (MIRINAE_USE_DALBARAGI_SUBMODULE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/dalbaragi)
else()
    find_package(dalbaragi CONFIG REQUIRED)
endif()

find_package(EnTT CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(uni-algo CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)


add_subdirectory(lib)

if (MIRINAE_BUILD_APP)
    add_subdirectory(app)
endif()

if (MIRINAE_ENABLE_TEST)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(test)
endif()
