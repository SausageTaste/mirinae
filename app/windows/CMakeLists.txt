add_library(mirinapp_windows STATIC
    dump.cpp
)
target_include_directories(mirinapp_windows PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(mirinapp_windows
    dbghelp.lib
)
add_library(mirinapp::windows ALIAS mirinapp_windows)

add_executable(mirinapp
    main.cpp
)
target_link_libraries(mirinapp
    glfw
    mirinae::core
    mirinapp::windows
)
