find_package(glfw3 CONFIG REQUIRED)


add_library(mirinapp_windows STATIC dump.cpp)
target_include_directories(mirinapp_windows PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mirinapp_windows sungtools::sungtools_basic)
add_library(mirinapp::windows ALIAS mirinapp_windows)
if (WIN32)
    target_link_libraries(mirinapp_windows dbghelp.lib)
endif()


add_executable(mirinapp main.cpp)
target_link_libraries(mirinapp
    glfw
    mirinae::core
    mirinapp::windows
)
