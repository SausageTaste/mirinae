add_library(mirinae_vulkan STATIC
    ./src/overlay/iwidget.cpp
    ./src/overlay/overlay.cpp
    ./src/overlay/text.cpp
    ./src/render/mem_alloc.cpp
    ./src/render/meshdata.cpp
    ./src/render/renderee.cpp
    ./src/render/renderpass.cpp
    ./src/render/uniform.cpp
    ./src/render/vkcomposition.cpp
    ./src/render/vkdevice.cpp
    ./src/render/vkmajorplayers.cpp
    ./src/renderer.cpp
)
add_library(mirinae::vulkan ALIAS mirinae_vulkan)
target_include_directories(mirinae_vulkan PUBLIC ./include)
target_compile_features(mirinae_vulkan PUBLIC cxx_std_17)
target_link_libraries(mirinae_vulkan
    PUBLIC
        Vulkan::Vulkan
        VulkanMemoryAllocator
        mirinae::cosmos
)

if (WIN32)
    target_compile_definitions(mirinae_vulkan PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
