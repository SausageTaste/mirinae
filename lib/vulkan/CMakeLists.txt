add_library(mirinae_vulkan STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/iwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/overlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay/text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/cmdbuf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/draw_set.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/enum_str.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/mem_alloc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/meshdata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/render_graph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderee.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderpass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/uniform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/vkcomposition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/vkdevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/vkmajorplayers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderee/terrain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/atmos/multi_scat_cs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/atmos/trans_lut.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/compo/dlight.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/compo/envmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/compo/sky.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/compo/slight.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/envmap/cubemap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/envmap/envmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/envmap/rp_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/envmap/rp_brdf_lut.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/envmap/rp_diffuse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/envmap/rp_sky.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/envmap/rp_specular.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/gbuf/gbuf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/gbuf/skinned.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/gbuf/static.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/gbuf/terrain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/misc/debug.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/ocean/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/ocean/h0.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/ocean/hkt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/ocean/ift.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/ocean/post_ift.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/ocean/render_tess.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/shadow/bundles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/shadow/skinned_trs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/shadow/skinned.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/shadow/static_trs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/shadow/static.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/shadow/terrain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/transp/skinned.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderpass/transp/static.cpp
)
target_include_directories(mirinae_vulkan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_precompile_headers(mirinae_vulkan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mirinae/vulkan_pch.h
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
