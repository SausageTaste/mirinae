add_library(mirinae_cosmos STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene_2/scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cosmos.cpp
)
add_library(mirinae::cosmos ALIAS mirinae_cosmos)
target_include_directories(mirinae_cosmos PUBLIC ./include)
target_compile_features(mirinae_cosmos PUBLIC cxx_std_17)
target_link_libraries(mirinae_cosmos PUBLIC
    EnTT::EnTT
    mirinae::aux
    mirinae::platform
)
