set(target_name mirinae_vulkan_base)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/mirinae/vulkan/base/context/base.hpp
    ${public_header_dir}/mirinae/vulkan/base/context/camera.hpp
    ${public_header_dir}/mirinae/vulkan/base/overlay/iwidget.hpp
    ${public_header_dir}/mirinae/vulkan/base/overlay/overlay.hpp
    ${public_header_dir}/mirinae/vulkan/base/overlay/text.hpp
    ${public_header_dir}/mirinae/vulkan/base/platform_func.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/cmdbuf.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/draw_set.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/enum_str.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/mem_alloc.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/mem_cinfo.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/meshdata.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/render_graph.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/renderee.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/renderpass.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/texture.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/uniform.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/vkcomposition.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/vkdebug.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/vkdevice.hpp
    ${public_header_dir}/mirinae/vulkan/base/render/vkmajorplayers.hpp
    ${public_header_dir}/mirinae/vulkan/base/renderee/atmos.hpp
    ${public_header_dir}/mirinae/vulkan/base/renderee/ren_actor_skinned.hpp
    ${public_header_dir}/mirinae/vulkan/base/renderee/terrain.hpp
    ${public_header_dir}/mirinae/vulkan/base/renderpass/builder.hpp
    ${public_header_dir}/mirinae/vulkan/base/renderpass/common.hpp
)

set(private_header_files
    ${private_header_dir}/render/device/phys_device.hpp
)

set(private_source_files
    ${private_source_dir}/context/camera.cpp
    ${private_source_dir}/overlay/iwidget.cpp
    ${private_source_dir}/overlay/overlay.cpp
    ${private_source_dir}/overlay/text.cpp
    ${private_source_dir}/render/cmdbuf.cpp
    ${private_source_dir}/render/device/phys_device.cpp
    ${private_source_dir}/render/draw_set.cpp
    ${private_source_dir}/render/enum_str.cpp
    ${private_source_dir}/render/mem_alloc.cpp
    ${private_source_dir}/render/mem_cinfo.cpp
    ${private_source_dir}/render/meshdata.cpp
    ${private_source_dir}/render/render_graph.cpp
    ${private_source_dir}/render/renderee.cpp
    ${private_source_dir}/render/renderpass.cpp
    ${private_source_dir}/render/texture.cpp
    ${private_source_dir}/render/uniform.cpp
    ${private_source_dir}/render/vkcomposition.cpp
    ${private_source_dir}/render/vkdebug.cpp
    ${private_source_dir}/render/vkdevice.cpp
    ${private_source_dir}/render/vkmajorplayers.cpp
    ${private_source_dir}/renderee/atmos.cpp
    ${private_source_dir}/renderee/ren_actor_skinned.cpp
    ${private_source_dir}/renderee/terrain.cpp
    ${private_source_dir}/renderpass/builder.cpp
    ${private_source_dir}/renderpass/common.cpp
)


add_library(${target_name} STATIC)
add_library(mirinae::vulkan_base ALIAS ${target_name})
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
        GPUOpen::VulkanMemoryAllocator
        mirinae::cosmos
        Vulkan::Vulkan
)

if (WIN32)
    target_compile_definitions(${target_name} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
