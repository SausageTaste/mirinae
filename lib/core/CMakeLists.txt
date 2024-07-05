add_library(mirinae_core STATIC
    ./src/mirinae/scene/transform.cpp
    ./src/mirinae/engine.cpp
)
add_library(mirinae::core ALIAS mirinae_core)
target_include_directories(mirinae_core PUBLIC ./include)
target_compile_features(mirinae_core PUBLIC cxx_std_17)
target_link_libraries(mirinae_core
    PUBLIC
        mirinae::vulkan
)

if (WIN32)
    target_compile_definitions(mirinae_core PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
