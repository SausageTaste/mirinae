include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)


FetchContent_Declare(
    fetch_vma
    GIT_REPOSITORY  https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG         v3.1.0
    GIT_PROGRESS    TRUE
)

FetchContent_Declare(
    fetch_lua
    GIT_REPOSITORY  https://github.com/lua/lua
    GIT_TAG         v5.4.6
    BUILD_COMMAND   ""
    GIT_PROGRESS    TRUE
    CONFIGURE_COMMAND ""
)

FetchContent_Declare(
    fetch_unialgo
    GIT_REPOSITORY  https://github.com/uni-algo/uni-algo
    GIT_TAG         v1.2.0
    GIT_PROGRESS    TRUE
)

FetchContent_Declare(
    fetch_entt
    GIT_REPOSITORY  https://github.com/skypjack/entt
    GIT_TAG         v3.13.2
    GIT_PROGRESS    TRUE
)

FetchContent_Declare(
    fetch_enet
    GIT_REPOSITORY  https://github.com/lsalzman/enet
    GIT_TAG         v1.3.18
    GIT_PROGRESS    TRUE
)

FetchContent_MakeAvailable(
    fetch_vma
    fetch_lua
    fetch_unialgo
    fetch_entt
    fetch_enet
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

add_subdirectory(../extern/DalbaragiTools  ${PROJECT_BINARY_DIR}/DalbaragiTools)

add_subdirectory(auxiliary)
add_subdirectory(platform)
add_subdirectory(cosmos)
add_subdirectory(vulkan)
add_subdirectory(core)
