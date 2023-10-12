add_executable(mirinapp
    main.cpp
)
target_link_libraries(mirinapp
    glfw
    mirinae::core
)
