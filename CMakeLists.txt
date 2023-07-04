cmake_minimum_required(VERSION 3.12)
project(mirinae)


add_library(mirinae
    ./src/engine.cpp
)
target_include_directories(mirinae PUBLIC ./include)
target_compile_features(mirinae PRIVATE cxx_std_17)


add_executable(mirinapp
    ./src/main.cpp
)
target_link_libraries(mirinapp mirinae)
