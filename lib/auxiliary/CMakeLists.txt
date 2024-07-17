add_library(mirinae_aux STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/input_proc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/network.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/script.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/skin_anim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/text_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/math/mamath.cpp
)
target_include_directories(mirinae_aux PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${fetch_enet_SOURCE_DIR}/include
)
target_link_libraries(mirinae_aux PUBLIC
    lua::lua
    uni-algo::uni-algo
    enet
    dalbaragi::daltools
    sungtools::general
)
target_compile_features(mirinae_aux PUBLIC cxx_std_17)
add_library(mirinae::aux ALIAS mirinae_aux)

if (WIN32)
    target_link_libraries(mirinae_aux PRIVATE ws2_32.lib winmm.lib)
endif (WIN32)
