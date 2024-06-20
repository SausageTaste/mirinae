include(FetchContent)


FetchContent_Declare(
    fetch_spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.14.1
)

FetchContent_Declare(
    fetch_vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG        v3.1.0
)

FetchContent_Declare(
    fetch_stb
    GIT_REPOSITORY    https://github.com/nothings/stb
    GIT_TAG           013ac3beddff3dbffafd5177e7972067cd2b5083
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

FetchContent_Declare(
    fetch_unialgo
    GIT_REPOSITORY  https://github.com/uni-algo/uni-algo
    GIT_TAG         v1.2.0
)

FetchContent_Declare(
    fetch_entt
    GIT_REPOSITORY  https://github.com/skypjack/entt
    GIT_TAG         v3.13.2
)

FetchContent_MakeAvailable(
    fetch_spdlog
    fetch_vma
    fetch_stb
    fetch_lua
    fetch_unialgo
    fetch_entt
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

add_subdirectory(../extern/SungToolsCpp  ${PROJECT_BINARY_DIR}/SungToolsCpp)
add_subdirectory(../extern/DalbaragiTools  ${PROJECT_BINARY_DIR}/DalbaragiTools)

add_subdirectory(auxiliary)
add_subdirectory(platform)
add_subdirectory(cosmos)
add_subdirectory(core)
