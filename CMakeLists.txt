cmake_minimum_required(VERSION 3.12)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(mirinae)

set(MIRINAE_ENABLE_TEST OFF CACHE BOOL "Enable testing")
set(MIRINAE_BUILD_APP OFF CACHE BOOL "Build projects in app folder")


add_subdirectory(extern/DalbaragiTools)

find_package(EnTT CONFIG REQUIRED)
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
