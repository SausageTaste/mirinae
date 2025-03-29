add_library(mirinae_vulkan STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/iwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/overlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/cmdbuf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/mem_alloc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/meshdata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/render_graph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderee.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass/builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass/compo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass/envmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass/gbuf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass/ocean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass/shadow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/uniform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/vkcomposition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/vkdevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/vkmajorplayers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderee/terrain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.cpp
)
target_include_directories(mirinae_vulkan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mirinae_vulkan PUBLIC
    GPUOpen::VulkanMemoryAllocator
    mirinae::cosmos
    Vulkan::Vulkan
)
target_compile_features(mirinae_vulkan PUBLIC cxx_std_17)
add_library(mirinae::vulkan ALIAS mirinae_vulkan)

if (WIN32)
    target_compile_definitions(mirinae_vulkan PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
