set(target_name mirinae_engine)
set(public_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(private_header_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(private_source_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(public_header_files
    ${public_header_dir}/mirinae/engine.hpp
)

set(private_header_files
    ${private_header_dir}/imgui_widget.hpp
    ${private_header_dir}/spawn_entt.hpp
)

set(private_source_files
    ${private_source_dir}/engine.cpp
    ${private_source_dir}/imgui_widget.cpp
    ${private_source_dir}/spawn_entt.cpp
)


add_library(${target_name} STATIC)
add_library(mirinae::engine ALIAS ${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "mirinae/engine")

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
        ${MINIAUDIO_INCLUDE_DIRS}
        ${private_header_dir}
)

target_link_libraries(${target_name}
    PUBLIC
        mirinae::vulkan_renderer
)
