set(target_name mirinae_vulkan_renpass_compo)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/mirinae/vulkan/renpass/compo/compo.hpp
)

set(private_source_files
    ${private_source_dir}/atmos_surface.cpp
    ${private_source_dir}/dlight.cpp
    ${private_source_dir}/envmap.cpp
    ${private_source_dir}/sky.cpp
    ${private_source_dir}/sky_atmos.cpp
    ${private_source_dir}/slight.cpp
    ${private_source_dir}/vplight.cpp
)


add_library(${target_name} STATIC)
add_library(mirinae::vulkan_renpass_compo ALIAS ${target_name})
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
