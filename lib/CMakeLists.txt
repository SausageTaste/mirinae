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
    GIT_TAG        8f516182776b028a3e268db89402f0bd6d4d6ec7
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

FetchContent_Declare(
    fetch_lua
    GIT_REPOSITORY    https://github.com/lua/lua
    GIT_TAG           v5.4.6
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
)

FetchContent_MakeAvailable(
    fetch_spdlog
    fetch_daltools
    fetch_sungtools
    fetch_vma
    fetch_stb
    fetch_lua
)

find_package(Vulkan REQUIRED)

# Make Lua library target
file(GLOB lua_source_files ${fetch_lua_SOURCE_DIR}/*.c)
list(FILTER lua_source_files EXCLUDE REGEX "lua.c$")
add_library(liblua STATIC ${lua_source_files})
add_library(lua::lua ALIAS liblua)
target_include_directories(liblua PUBLIC ${fetch_lua_SOURCE_DIR})

# define global macro
add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)


add_subdirectory(platform)
add_subdirectory(core)
