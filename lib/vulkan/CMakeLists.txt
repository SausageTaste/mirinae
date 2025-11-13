set(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(src_files
    ${src_dir}/context/camera.cpp
    ${src_dir}/overlay/iwidget.cpp
    ${src_dir}/overlay/overlay.cpp
    ${src_dir}/overlay/text.cpp
    ${src_dir}/render/cmdbuf.cpp
    ${src_dir}/render/draw_set.cpp
    ${src_dir}/render/enum_str.cpp
    ${src_dir}/render/mem_alloc.cpp
    ${src_dir}/render/mem_cinfo.cpp
    ${src_dir}/render/meshdata.cpp
    ${src_dir}/render/render_graph.cpp
    ${src_dir}/render/renderee.cpp
    ${src_dir}/render/renderpass.cpp
    ${src_dir}/render/texture.cpp
    ${src_dir}/render/uniform.cpp
    ${src_dir}/render/vkcomposition.cpp
    ${src_dir}/render/vkdebug.cpp
    ${src_dir}/render/vkdevice.cpp
    ${src_dir}/render/vkmajorplayers.cpp
    ${src_dir}/renderee/atmos.cpp
    ${src_dir}/renderee/ren_actor_skinned.cpp
    ${src_dir}/renderee/terrain.cpp
    ${src_dir}/renderer.cpp
    ${src_dir}/renderpass/atmos/cam_volumes.cpp
    ${src_dir}/renderpass/atmos/common.cpp
    ${src_dir}/renderpass/atmos/multi_scat_cs.cpp
    ${src_dir}/renderpass/atmos/sky_view_lut.cpp
    ${src_dir}/renderpass/atmos/trans_lut.cpp
    ${src_dir}/renderpass/bloom/blend.cpp
    ${src_dir}/renderpass/bloom/downsample.cpp
    ${src_dir}/renderpass/bloom/upsample.cpp
    ${src_dir}/renderpass/builder.cpp
    ${src_dir}/renderpass/common.cpp
    ${src_dir}/renderpass/compo/atmos_surface.cpp
    ${src_dir}/renderpass/compo/dlight.cpp
    ${src_dir}/renderpass/compo/envmap.cpp
    ${src_dir}/renderpass/compo/sky_atmos.cpp
    ${src_dir}/renderpass/compo/sky.cpp
    ${src_dir}/renderpass/compo/slight.cpp
    ${src_dir}/renderpass/compo/vplight.cpp
    ${src_dir}/renderpass/envmap/cubemap.cpp
    ${src_dir}/renderpass/envmap/diffuse.cpp
    ${src_dir}/renderpass/envmap/envmap.cpp
    ${src_dir}/renderpass/envmap/mip_chain.cpp
    ${src_dir}/renderpass/envmap/rp_base.cpp
    ${src_dir}/renderpass/envmap/rp_brdf_lut.cpp
    ${src_dir}/renderpass/envmap/rp_diffuse.cpp
    ${src_dir}/renderpass/envmap/rp_sky.cpp
    ${src_dir}/renderpass/envmap/rp_specular.cpp
    ${src_dir}/renderpass/envmap/sky_atmos.cpp
    ${src_dir}/renderpass/envmap/specular.cpp
    ${src_dir}/renderpass/gbuf/gbuf.cpp
    ${src_dir}/renderpass/gbuf/static.cpp
    ${src_dir}/renderpass/gbuf/terrain.cpp
    ${src_dir}/renderpass/misc/debug.cpp
    ${src_dir}/renderpass/misc/skin_anim.cpp
    ${src_dir}/renderpass/ocean/common.cpp
    ${src_dir}/renderpass/ocean/h0.cpp
    ${src_dir}/renderpass/ocean/hkt.cpp
    ${src_dir}/renderpass/ocean/ift.cpp
    ${src_dir}/renderpass/ocean/post_ift.cpp
    ${src_dir}/renderpass/ocean/render_tess.cpp
    ${src_dir}/renderpass/shadow/bundles.cpp
    ${src_dir}/renderpass/shadow/static_trs.cpp
    ${src_dir}/renderpass/shadow/static.cpp
    ${src_dir}/renderpass/shadow/terrain.cpp
    ${src_dir}/renderpass/transp/static.cpp
)


add_library(mirinae_vulkan STATIC ${src_files})
add_library(mirinae::vulkan ALIAS mirinae_vulkan)
target_include_directories(mirinae_vulkan
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_precompile_headers(mirinae_vulkan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_pch.h
)
target_link_libraries(mirinae_vulkan PUBLIC
    GPUOpen::VulkanMemoryAllocator
    mirinae::cosmos
    Vulkan::Vulkan
)
set_target_properties(mirinae_vulkan PROPERTIES
    FOLDER "mirinae"
)

if (WIN32)
    target_compile_definitions(mirinae_vulkan PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
