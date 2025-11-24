set(target_name mirinae_vulkan_renpass_bloom)
set(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS
    "${src_dir}/*.cpp"
)

file(GLOB_RECURSE private_headers CONFIGURE_DEPENDS
    "${src_dir}/*.h"
    "${src_dir}/*.hpp"
)

file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS
    "${include_dir}/*.h"
    "${include_dir}/*.hpp"
)


add_library(${target_name} STATIC)
add_library(mirinae::vulkan_renpass_bloom ALIAS ${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "mirinae/vulkan/renpass")

target_sources(${target_name}
    PUBLIC
        ${public_headers}
    PRIVATE
        ${private_headers}
        ${source_files}
)

target_include_directories(${target_name}
    PUBLIC
        "${include_dir}"
    PRIVATE
        "${src_dir}"
)

target_link_libraries(${target_name} PUBLIC
    mirinae::vulkan_base
)

if (WIN32)
    target_compile_definitions(${target_name} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
