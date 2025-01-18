add_library(mirinae_platform STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osio.cpp
)
target_include_directories(mirinae_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(mirinae_platform PUBLIC cxx_std_17)
add_library(mirinae::platform ALIAS mirinae_platform)
