include(FetchContent)


FetchContent_Declare(
    fetch_spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.12.0
)

FetchContent_Declare(
    fetch_daltools
    GIT_REPOSITORY https://github.com/SausageTaste/DalbaragiTools
    GIT_TAG        edc9d2fa242afda277c5331185cdb58dc603c61b
)

FetchContent_Declare(
    fetch_sungtools
    GIT_REPOSITORY https://github.com/SausageTaste/SungToolsCpp
    GIT_TAG        db00620692aafc293412fcfee914fd465f3bb08b
)

FetchContent_Declare(
    fetch_vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.0.1
)

FetchContent_Declare(
    fetch_stb
    GIT_REPOSITORY    https://github.com/nothings/stb
    GIT_TAG           f4a71b13373436a2866c5d68f8f80ac6f0bc1ffe
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
)

FetchContent_MakeAvailable(
    fetch_spdlog
    fetch_daltools
    fetch_sungtools
    fetch_vma
    fetch_stb
)

find_package(Vulkan REQUIRED)

# define global macro
add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)


add_subdirectory(platform)
add_subdirectory(core)
