set(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(src_files
    ${src_dir}/lightweight/input_proc.cpp
    ${src_dir}/lightweight/network.cpp
    ${src_dir}/lightweight/script.cpp
    ${src_dir}/lightweight/skin_anim.cpp
    ${src_dir}/lightweight/task.cpp
    ${src_dir}/lightweight/text_data.cpp
    ${src_dir}/math/color.cpp
    ${src_dir}/math/mamath.cpp
)


add_library(mirinae_aux STATIC ${src_files})
add_library(mirinae::aux ALIAS mirinae_aux)
target_include_directories(mirinae_aux PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(mirinae_aux PUBLIC
    ${LUA_LIBRARIES}
    dalbaragi::dalbaragi_parser
    uni-algo::uni-algo
)
target_compile_features(mirinae_aux PUBLIC cxx_std_17)
set_target_properties(mirinae_aux PROPERTIES
    FOLDER "mirinae"
)

if (WIN32)
    target_link_libraries(mirinae_aux PRIVATE ws2_32.lib winmm.lib)
endif (WIN32)
