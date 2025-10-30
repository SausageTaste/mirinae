find_package(SDL3 CONFIG REQUIRED)


add_executable(mirinapp_sdl main.cpp)
target_link_libraries(mirinapp_sdl
    SDL3::SDL3
    mirinae::core
)
set_target_properties(mirinapp_sdl PROPERTIES
    FOLDER "mirinae/app"
)
