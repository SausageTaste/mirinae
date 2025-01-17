if (WIN32)
    add_subdirectory(windows)
elseif(UNIX AND NOT APPLE)
    add_subdirectory(linux)
endif()

add_subdirectory(server)
