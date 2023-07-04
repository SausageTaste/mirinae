cmake_minimum_required(VERSION 3.12)
project(mirinae)

include(FetchContent)


FetchContent_Declare(
    dal_glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.8
)

FetchContent_MakeAvailable(dal_glfw)


add_library(mirinae
    ./src/engine.cpp
)
target_include_directories(mirinae PUBLIC ./include)
target_compile_features(mirinae PRIVATE cxx_std_17)
target_link_libraries(mirinae
    PRIVATE
        glfw
)


add_executable(mirinapp
    ./src/main.cpp
)
target_link_libraries(mirinapp mirinae)
