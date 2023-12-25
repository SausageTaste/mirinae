add_library(mirinae_platform STATIC
    ./src/filesys.cpp
)
add_library(mirinae::platform ALIAS mirinae_platform)
target_include_directories(mirinae_platform PUBLIC ./include)
target_compile_features(mirinae_platform PUBLIC cxx_std_17)
