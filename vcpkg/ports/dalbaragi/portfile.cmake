vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO SausageTaste/dalbaragi
    REF main
    SHA512 094e2cb4e441b88ff37c22cae60ddad3de2ace026586ed60ee3544eae96463e656cf3ed6e09ee951c03fa51bf1b790be39d1c191bd00bce5d86c1348c1cde645
)

vcpkg_install_copyright(FILE_LIST ${SOURCE_PATH}/LICENSE)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DCMAKE_INSTALL_PREFIX=${CURRENT_PACKAGES_DIR}
)

vcpkg_install_cmake()
