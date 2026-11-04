# PyroWave, remote play's video codec (Vulkan compute wavelets, intra-only): the pinned fork's C API,
# `libpyrowave-shared`, from the shared third-party prefix (cmake/thirdparty, built by
# `tools.check deps`), found as `find_package(OpensuPyroWave)`. The library is staged beside
# librashader in the build tree and installed there, `<prefix>/<libdir>/opensu/`, which openSU's
# RPATH names.
#
# OPENSU_BUILD_PYROWAVE=OFF builds without it: remote play then refuses to start with the reason.
# Built on Linux, the one system whose host side (Gamescope's PipeWire stream) exists.

include(GNUInstallDirs)
include(ThirdParty)

option(OPENSU_BUILD_PYROWAVE
    "Use the pinned PyroWave fork remote play encodes and decodes video with. OFF builds openSU \
without remote play's video" ON)

set(OPENSU_PYROWAVE_DIR "${CMAKE_INSTALL_LIBDIR}/opensu")

function(opensu_add_pyrowave)
    opensu_require_thirdparty(OpensuPyroWave "${OPENSU_PYROWAVE_COMMIT}" OPENSU_BUILD_PYROWAVE)
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_PYROWAVE_DIR}/${OpensuPyroWave_SONAME}")
    add_custom_command(OUTPUT "${staged}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OpensuPyroWave_LIBRARY}" "${staged}"
        DEPENDS "${OpensuPyroWave_LIBRARY}"
        COMMENT "Staging PyroWave"
        VERBATIM)
    add_custom_target(opensu_pyrowave DEPENDS "${staged}")

    add_library(opensu_pyrowave_library SHARED IMPORTED GLOBAL)
    add_library(opensu::pyrowave ALIAS opensu_pyrowave_library)
    set_target_properties(opensu_pyrowave_library PROPERTIES
        IMPORTED_LOCATION "${staged}"
        IMPORTED_SONAME "${OpensuPyroWave_SONAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${OpensuPyroWave_INCLUDE_DIR}")

    install(FILES "${staged}" DESTINATION "${OPENSU_PYROWAVE_DIR}")
endfunction()

if(OPENSU_BUILD_PYROWAVE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    opensu_add_pyrowave()
endif()
