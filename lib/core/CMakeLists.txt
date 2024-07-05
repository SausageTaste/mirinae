add_library(mirinae_core STATIC
    ./src/mirinae/overlay/iwidget.cpp
    ./src/mirinae/overlay/overlay.cpp
    ./src/mirinae/overlay/text.cpp
    ./src/mirinae/render/mem_alloc.cpp
    ./src/mirinae/render/meshdata.cpp
    ./src/mirinae/render/renderee.cpp
    ./src/mirinae/render/renderpass.cpp
    ./src/mirinae/render/uniform.cpp
    ./src/mirinae/render/vkcomposition.cpp
    ./src/mirinae/render/vkdevice.cpp
    ./src/mirinae/render/vkmajorplayers.cpp
    ./src/mirinae/scene/transform.cpp
    ./src/mirinae/util/image.cpp
    ./src/mirinae/util/input_proc.cpp
    ./src/mirinae/engine.cpp
)
add_library(mirinae::core ALIAS mirinae_core)
target_include_directories(mirinae_core PUBLIC ./include ${fetch_stb_SOURCE_DIR})
target_compile_features(mirinae_core PUBLIC cxx_std_17)
target_link_libraries(mirinae_core
    PUBLIC
        Vulkan::Vulkan
        mirinae::cosmos
    PRIVATE
        VulkanMemoryAllocator
)

if (WIN32)
    target_compile_definitions(mirinae_core PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
