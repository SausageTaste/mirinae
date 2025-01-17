vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO SausageTaste/dalbaragi
    REF v0.1.2
    SHA512 2bddf960fdc1161d3fdd67a11ecd78aee22e225a4c3e18360159ecd5eed1f240e22c5b4c412fa6f49a7ca6adde58dc52ff03d65859963274b18016a9e78495cb
)

vcpkg_install_copyright(FILE_LIST ${SOURCE_PATH}/LICENSE)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DCMAKE_INSTALL_PREFIX=${CURRENT_PACKAGES_DIR}
)

vcpkg_install_cmake()
