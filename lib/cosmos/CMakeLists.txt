add_library(mirinae_cosmos STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cosmos.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/envmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/identifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/light.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/ocean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/ren_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/terrain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpnt/transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/phys_world.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene.cpp
)
add_library(mirinae::cosmos ALIAS mirinae_cosmos)
target_include_directories(mirinae_cosmos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mirinae_cosmos PUBLIC cxx_std_17)
target_link_libraries(mirinae_cosmos PUBLIC
    EnTT::EnTT
    imgui::imgui
    Jolt::Jolt
    mirinae::aux
)
