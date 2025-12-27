set(target_name mirinae_vulkan_renderer)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/mirinae/vulkan/renderer.hpp
)

set(private_header_files
    ${private_header_dir}/renderpasses.hpp
    ${private_header_dir}/task/init_model.hpp
    ${private_header_dir}/task/update_dlight.hpp
    ${private_header_dir}/task/update_ren_ctxt.hpp
    ${private_header_dir}/util/flags.hpp
    ${private_header_dir}/util/frame_sync.hpp
    ${private_header_dir}/vulkan_pch.h
)

set(private_source_files
    ${private_source_dir}/renderer.cpp
    ${private_source_dir}/renderpasses.cpp
    ${private_source_dir}/task/init_model.cpp
    ${private_source_dir}/task/update_dlight.cpp
    ${private_source_dir}/task/update_ren_ctxt.cpp
    ${private_source_dir}/util/frame_sync.cpp
)


add_library(${target_name} STATIC)
add_library(mirinae::vulkan_renderer ALIAS ${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "mirinae/engine/vulkan")

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
        mirinae::vulkan_base
    PRIVATE
        mirinae::vulkan_renpass_atmos
        mirinae::vulkan_renpass_bloom
        mirinae::vulkan_renpass_compo
        mirinae::vulkan_renpass_envmap
        mirinae::vulkan_renpass_gbuf
        mirinae::vulkan_renpass_misc
        mirinae::vulkan_renpass_ocean
        mirinae::vulkan_renpass_shadow
        mirinae::vulkan_renpass_transp
)
