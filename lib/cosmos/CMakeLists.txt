set(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(src_files
    ${src_dir}/cosmos.cpp
    ${src_dir}/cpnt/atmos_entt.cpp
    ${src_dir}/cpnt/atmos_lua.cpp
    ${src_dir}/cpnt/camera.cpp
    ${src_dir}/cpnt/envmap.cpp
    ${src_dir}/cpnt/identifier.cpp
    ${src_dir}/cpnt/light.cpp
    ${src_dir}/cpnt/ocean.cpp
    ${src_dir}/cpnt/ren_model.cpp
    ${src_dir}/cpnt/terrain.cpp
    ${src_dir}/cpnt/transform.cpp
    ${src_dir}/scene/jolt_job_sys.cpp
    ${src_dir}/scene/phys_world.cpp
    ${src_dir}/scene/scene.cpp
)


add_library(mirinae_cosmos STATIC ${src_files})
add_library(mirinae::cosmos ALIAS mirinae_cosmos)
target_include_directories(mirinae_cosmos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mirinae_cosmos PUBLIC
    EnTT::EnTT
    imgui::imgui
    Jolt::Jolt
    mirinae::aux
)
set_target_properties(mirinae_cosmos PROPERTIES
    FOLDER "mirinae"
)

set_source_files_properties(
    ${src_dir}/scene/jolt_job_sys.cpp
    PROPERTIES COMPILE_FLAGS "-fno-rtti"
)
