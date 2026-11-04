# The Gamescope fork the nested session runs.
#
# Stock Gamescope answers every close request from the host compositor with SIGTERM, so
# Alt+F4 in KDE ends the whole session. The fork adds --close-focused-window, which sends
# the close to the focused game instead. It is built once per pinned commit and build image in the
# shared third-party prefix (cmake/thirdparty, built by `tools.check deps`) and found here as
# `find_package(OpensuGamescope)`; a tree only copies the binary.
#
# Layout: the binary is staged at <build>/<libexecdir>/opensu/gamescope, which is where the
# running build-tree executable looks (<exe dir>/../<libexecdir>/opensu/gamescope), and it is
# installed to <prefix>/<libexecdir>/opensu/gamescope, where an installed opensu looks.

include(GNUInstallDirs)
include(ThirdParty)

option(OPENSU_BUILD_GAMESCOPE
    "Stage the pinned Gamescope fork the nested session runs. OFF only for development of \
the rest of opensu: the nested session then refuses to start" ON)

# Where the running opensu finds the binary, relative to the directory above its own.
set(OPENSU_GAMESCOPE_RELATIVE "${CMAKE_INSTALL_LIBEXECDIR}/opensu/gamescope")

function(opensu_add_gamescope)
    opensu_require_thirdparty(OpensuGamescope "${OPENSU_GAMESCOPE_COMMIT}" OPENSU_BUILD_GAMESCOPE)
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_GAMESCOPE_RELATIVE}")
    add_custom_command(OUTPUT "${staged}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OpensuGamescope_EXECUTABLE}" "${staged}"
        COMMAND "${CMAKE_COMMAND}" "-DBINARY=${staged}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GamescopeLddCheck.cmake"
        DEPENDS "${OpensuGamescope_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/cmake/GamescopeLddCheck.cmake"
        COMMENT "Staging Gamescope"
        VERBATIM)
    add_custom_target(opensu_gamescope DEPENDS "${staged}")

    install(PROGRAMS "${staged}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/opensu")
endfunction()
