set(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(src_files
    ${src_dir}/engine.cpp
)


add_library(mirinae_engine STATIC ${src_files})
add_library(mirinae::engine ALIAS mirinae_engine)
target_include_directories(mirinae_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mirinae_engine PUBLIC
    mirinae::vulkan
)
set_target_properties(mirinae_engine PROPERTIES
    FOLDER "mirinae"
)

if (WIN32)
    target_compile_definitions(mirinae_engine PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif (WIN32)
