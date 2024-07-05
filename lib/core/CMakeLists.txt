add_library(mirinae_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp
)
target_include_directories(mirinae_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mirinae_core PUBLIC
    mirinae::vulkan
)
target_compile_features(mirinae_core PUBLIC cxx_std_17)
add_library(mirinae::core ALIAS mirinae_core)

if (WIN32)
    target_compile_definitions(mirinae_core PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
