cmake_minimum_required(VERSION 3.12)
project(mirinae)

include(FetchContent)


FetchContent_Declare(
    dal_glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.8
)
option(GLFW_BUILD_DOCS off)
option(GLFW_BUILD_EXAMPLES off)
option(GLFW_BUILD_TESTS off)
option(GLFW_USE_HYBRID_HPG on)

FetchContent_Declare(
    dal_spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.11.0
)

FetchContent_Declare(
    fetch_daltools
    GIT_REPOSITORY https://github.com/SausageTaste/DalbaragiTools
    GIT_TAG        main
)

FetchContent_Declare(
    fetch_stb
    GIT_REPOSITORY    https://github.com/nothings/stb
    GIT_TAG           master
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
)

FetchContent_MakeAvailable(
    dal_glfw
    dal_spdlog
    fetch_daltools
    fetch_stb
)

find_package(Vulkan REQUIRED)


add_library(mirinae
    ./src/mirinae/render/vkmajorplayers.cpp
    ./src/mirinae/engine.cpp
)
target_include_directories(mirinae PUBLIC ./include ${fetch_stb_SOURCE_DIR})
target_compile_features(mirinae PUBLIC cxx_std_17)
target_link_libraries(mirinae
    PRIVATE
        Vulkan::Vulkan
        spdlog
        glfw
        dalbaragi::daltools
)
target_compile_definitions(mirinae PRIVATE VK_USE_PLATFORM_WIN32_KHR)


add_executable(mirinapp
    ./src/main.cpp
)
target_link_libraries(mirinapp mirinae)
