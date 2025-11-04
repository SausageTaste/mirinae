set(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(src_files
    ${src_dir}/engine.cpp
)


add_library(mirinae_core STATIC ${src_files})
add_library(mirinae::core ALIAS mirinae_core)
target_include_directories(mirinae_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mirinae_core PUBLIC
    mirinae::vulkan
)
set_target_properties(mirinae_core PROPERTIES
    FOLDER "mirinae"
)

if (WIN32)
    target_compile_definitions(mirinae_core PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
