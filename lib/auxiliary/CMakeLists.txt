add_library(mirinae_aux STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/input_proc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/network.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/script.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/skin_anim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/text_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/math/color.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/math/mamath.cpp
)
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
add_library(mirinae::aux ALIAS mirinae_aux)

if (WIN32)
    target_link_libraries(mirinae_aux PRIVATE ws2_32.lib winmm.lib)
endif (WIN32)
