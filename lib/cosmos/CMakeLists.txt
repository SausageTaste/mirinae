set(target_name mirinae_cosmos)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/mirinae/cosmos.hpp
    ${public_header_dir}/mirinae/cpnt/atmos.hpp
    ${public_header_dir}/mirinae/cpnt/camera.hpp
    ${public_header_dir}/mirinae/cpnt/common.hpp
    ${public_header_dir}/mirinae/cpnt/envmap.hpp
    ${public_header_dir}/mirinae/cpnt/identifier.hpp
    ${public_header_dir}/mirinae/cpnt/light.hpp
    ${public_header_dir}/mirinae/cpnt/ocean.hpp
    ${public_header_dir}/mirinae/cpnt/phys_body.hpp
    ${public_header_dir}/mirinae/cpnt/ren_model.hpp
    ${public_header_dir}/mirinae/cpnt/terrain.hpp
    ${public_header_dir}/mirinae/cpnt/transform.hpp
    ${public_header_dir}/mirinae/scene/jolt_job_sys.hpp
    ${public_header_dir}/mirinae/scene/phys_world.hpp
    ${public_header_dir}/mirinae/scene/scene.hpp
)

set(private_source_files
    ${private_source_dir}/cosmos.cpp
    ${private_source_dir}/cpnt/atmos_entt.cpp
    ${private_source_dir}/cpnt/atmos_lua.cpp
    ${private_source_dir}/cpnt/camera.cpp
    ${private_source_dir}/cpnt/envmap.cpp
    ${private_source_dir}/cpnt/identifier.cpp
    ${private_source_dir}/cpnt/light.cpp
    ${private_source_dir}/cpnt/ocean.cpp
    ${private_source_dir}/cpnt/phys_body.cpp
    ${private_source_dir}/cpnt/ren_model.cpp
    ${private_source_dir}/cpnt/terrain.cpp
    ${private_source_dir}/cpnt/transform.cpp
    ${private_source_dir}/scene/jolt_job_sys.cpp
    ${private_source_dir}/scene/phys_world.cpp
    ${private_source_dir}/scene/scene.cpp
)


add_library(${target_name} STATIC)
add_library(mirinae::cosmos ALIAS ${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "mirinae/engine")

target_sources(${target_name}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ${public_header_dir}
        FILES
            ${public_header_files}
    PRIVATE
        ${private_header_files}
        ${private_source_files}
)

target_include_directories(${target_name}
    PUBLIC
        ${public_header_dir}
    PRIVATE
        ${private_header_dir}
)

target_link_libraries(${target_name}
    PUBLIC
        EnTT::EnTT
        imgui::imgui
        Jolt::Jolt
        mirinae::aux
)

set_source_files_properties(
    ${private_source_dir}/scene/jolt_job_sys.cpp
    PROPERTIES COMPILE_FLAGS "-fno-rtti"
)
