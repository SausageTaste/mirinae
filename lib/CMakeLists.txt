include(FetchContent)


FetchContent_Declare(
    fetch_spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.11.0
)

FetchContent_Declare(
    fetch_daltools
    GIT_REPOSITORY https://github.com/SausageTaste/DalbaragiTools
    GIT_TAG        main
)

FetchContent_Declare(
    fetch_sungtools
    GIT_REPOSITORY https://github.com/SausageTaste/SungToolsCpp
    GIT_TAG        ce94acda78e25a56c8d372672400d39f8ad7bbd6
)

FetchContent_Declare(
    fetch_stb
    GIT_REPOSITORY    https://github.com/nothings/stb
    GIT_TAG           master
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
)

FetchContent_Declare(
    fetch_vma
    GIT_REPOSITORY    https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG           v3.0.1
)

FetchContent_MakeAvailable(
    fetch_spdlog
    fetch_daltools
    fetch_sungtools
    fetch_stb
    fetch_vma
)

find_package(Vulkan REQUIRED)


add_subdirectory(platform)
add_subdirectory(core)