add_library(mirinae_aux STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/input_proc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/script.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/skin_anim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/text_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/math/mamath.cpp
)
target_include_directories(mirinae_aux PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${fetch_stb_SOURCE_DIR}
)
target_link_libraries(mirinae_aux PUBLIC
    glm::glm
    lua::lua
    uni-algo::uni-algo
    spdlog
    dalbaragi::daltools
    sungtools::general
)
target_compile_features(mirinae_aux PUBLIC cxx_std_17)
add_library(mirinae::aux ALIAS mirinae_aux)
