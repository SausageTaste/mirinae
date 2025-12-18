set(target_name mirinae_vulkan_renpass_misc)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/mirinae/vulkan/renpass/misc/misc.hpp
    ${public_header_dir}/mirinae/vulkan/renpass/misc/skin_anim.hpp
)

set(private_source_files
    ${private_source_dir}/debug.cpp
    ${private_source_dir}/skin_anim.cpp
)


add_library(${target_name} STATIC)
add_library(mirinae::vulkan_renpass_misc ALIAS ${target_name})
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
