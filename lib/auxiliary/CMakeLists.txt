add_library(mirinae_aux STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lightweight/time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/math/mamath.cpp
)
add_library(mirinae::aux ALIAS mirinae_aux)
target_include_directories(mirinae_aux PUBLIC ./include)
target_compile_features(mirinae_aux PUBLIC cxx_std_17)
target_link_libraries(mirinae_aux PUBLIC
    glm::glm
    spdlog
    sungtools::general
)
