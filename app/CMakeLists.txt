FetchContent_Declare(
    fetch_glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.8
)
option(GLFW_BUILD_DOCS off)
option(GLFW_BUILD_EXAMPLES off)
option(GLFW_BUILD_TESTS off)
option(GLFW_USE_HYBRID_HPG on)


FetchContent_MakeAvailable(
    fetch_glfw
)


add_subdirectory(windows)
