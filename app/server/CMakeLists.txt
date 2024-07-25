add_executable(mirinaebongsa
    main.cpp
)
target_link_libraries(mirinaebongsa mirinae::core)

if (WIN32)
    target_link_libraries(mirinaebongsa mirinapp::windows)
endif()
