add_executable(mirinaebongsa
    main.cpp
)
target_link_libraries(mirinaebongsa mirinae::core)
set_target_properties(mirinaebongsa PROPERTIES
    FOLDER "mirinae/app"
)

if (WIN32)
    target_link_libraries(mirinaebongsa mirinapp::windows)
endif()
