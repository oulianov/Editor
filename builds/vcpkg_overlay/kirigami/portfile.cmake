vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO KDE/kirigami
    REF "v${VERSION}"
    SHA512 6d9851a43741eb80e914b2a54cb6f171522eea9fd4095003d0db4b8a26af6dea26052fc647c3c5f83481fcc82f499574a63c5714abb472a543e4e35c585ce30c
    HEAD_REF master
)

vcpkg_replace_string("${SOURCE_PATH}/CMakeLists.txt" "configure_file(qmllint.ini.in" "#configure_file(qmllint.ini.in")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_EXAMPLES=OFF
        "-DCMAKE_PREFIX_PATH=$ENV{Qt6_Path}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME KF6Kirigami CONFIG_PATH lib/cmake/KF6Kirigami DO_NOT_DELETE_PARENT_CONFIG_PATH)
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6Kirigami2 CONFIG_PATH lib/cmake/KF6Kirigami2 DO_NOT_DELETE_PARENT_CONFIG_PATH)
vcpkg_cmake_config_fixup(PACKAGE_NAME KF6KirigamiPlatform CONFIG_PATH lib/cmake/KF6KirigamiPlatform)

# Remove debug files
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
  file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin")
endif()

# Handle copyright
vcpkg_install_copyright(FILE_LIST
    "${SOURCE_PATH}/LICENSES/BSD-2-Clause.txt"
    "${SOURCE_PATH}/LICENSES/BSD-3-Clause.txt"
    "${SOURCE_PATH}/LICENSES/CC0-1.0.txt"
    "${SOURCE_PATH}/LICENSES/FSFAP.txt"
    "${SOURCE_PATH}/LICENSES/GPL-2.0-or-later.txt"
    "${SOURCE_PATH}/LICENSES/LGPL-2.0-or-later.txt"
    "${SOURCE_PATH}/LICENSES/LGPL-2.1-only.txt"
    "${SOURCE_PATH}/LICENSES/LGPL-3.0-only.txt"
    "${SOURCE_PATH}/LICENSES/LicenseRef-KDE-Accepted-LGPL.txt"
    "${SOURCE_PATH}/LICENSES/MIT.txt"
)
