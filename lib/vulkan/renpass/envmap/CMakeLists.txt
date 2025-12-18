set(target_name mirinae_vulkan_renpass_envmap)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/mirinae/vulkan/renpass/envmap/envmap.hpp
)

set(private_header_files
    ${private_header_dir}/cubemap.hpp
    ${private_header_dir}/rp.hpp
)

set(private_source_files
    ${private_source_dir}/cubemap.cpp
    ${private_source_dir}/diffuse.cpp
    ${private_source_dir}/envmap.cpp
    ${private_source_dir}/mip_chain.cpp
    ${private_source_dir}/rp_base.cpp
    ${private_source_dir}/rp_brdf_lut.cpp
    ${private_source_dir}/rp_diffuse.cpp
    ${private_source_dir}/rp_sky.cpp
    ${private_source_dir}/rp_specular.cpp
    ${private_source_dir}/sky_atmos.cpp
    ${private_source_dir}/specular.cpp
)


add_library(${target_name} STATIC)
add_library(mirinae::vulkan_renpass_envmap ALIAS ${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "mirinae/engine/vulkan/renpass")

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

target_link_libraries(${target_name} PUBLIC
    mirinae::vulkan_base
)
