# The Gamescope fork the nested session runs.
#
# Stock Gamescope answers every close request from the host compositor with SIGTERM, so
# Alt+F4 in KDE ends the whole session. The fork adds --close-focused-window, which sends
# the close to the focused game instead. It is built from the pinned commit as an
# ExternalProject, with meson and ninja running inside a podman container built from
# packaging/gamescope-build/Containerfile, so the host needs no Gamescope build dependencies.
# The image is built once per Containerfile content, the fork once, and a second build does
# nothing.
#
# Layout: the binary is staged at <build>/<libexecdir>/iideck/gamescope, which is where the
# running build-tree executable looks (<exe dir>/../<libexecdir>/iideck/gamescope), and it is
# installed to <prefix>/<libexecdir>/iideck/gamescope, where an installed iideck looks.

include(ExternalProject)
include(GNUInstallDirs)

option(IIDECK_BUILD_GAMESCOPE
    "Build the pinned Gamescope fork the nested session runs. OFF only for development of \
the rest of iideck: the nested session then refuses to start" ON)

set(IIDECK_GAMESCOPE_REPOSITORY "https://github.com/SomeoneIsWorking/gamescope.git")
set(IIDECK_GAMESCOPE_COMMIT "41e84d4f5870a06534e310ff279a19a137375f79")

# Where the running iideck finds the binary, relative to the directory above its own.
set(IIDECK_GAMESCOPE_RELATIVE "${CMAKE_INSTALL_LIBEXECDIR}/iideck/gamescope")

set(IIDECK_GAMESCOPE_CONTAINERFILE "${CMAKE_SOURCE_DIR}/packaging/gamescope-build/Containerfile")

# The image is based on the host's Fedora release, so the binary links the host's libraries.
function(iideck_gamescope_image_tag out fedora_version_out)
    file(STRINGS /etc/os-release release REGEX "^(ID|VERSION_ID)=")
    set(id "")
    set(version "")
    foreach(line IN LISTS release)
        if(line MATCHES "^ID=\"?([^\"]*)\"?$")
            set(id "${CMAKE_MATCH_1}")
        elseif(line MATCHES "^VERSION_ID=\"?([^\"]*)\"?$")
            set(version "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT id STREQUAL "fedora" OR version STREQUAL "")
        message(FATAL_ERROR
            "The Gamescope fork is built in a Fedora container matching the host, so only "
            "Fedora hosts are supported (this host: '${id}'). Configure with "
            "-DIIDECK_BUILD_GAMESCOPE=OFF to work on the rest of iideck; the nested session "
            "then refuses to start.")
    endif()
    file(SHA256 "${IIDECK_GAMESCOPE_CONTAINERFILE}" digest)
    string(SUBSTRING "${digest}" 0 12 short)
    set(${out} "localhost/iideck-gamescope-build:fedora-${version}-${short}" PARENT_SCOPE)
    set(${fedora_version_out} "${version}" PARENT_SCOPE)
endfunction()

function(iideck_add_gamescope)
    find_program(IIDECK_PODMAN podman)
    if(NOT IIDECK_PODMAN)
        message(FATAL_ERROR
            "Building the Gamescope fork needs podman (rootless). Install it: `sudo dnf install "
            "podman`. To work on the rest of iideck without the fork, configure with "
            "-DIIDECK_BUILD_GAMESCOPE=OFF.")
    endif()
    iideck_gamescope_image_tag(image fedora_version)

    set(root "${CMAKE_BINARY_DIR}/gamescope")
    set(built "${root}/build/src/gamescope")
    set(staged "${CMAKE_BINARY_DIR}/${IIDECK_GAMESCOPE_RELATIVE}")
    set(image_stamp "${root}/image.stamp")

    add_custom_command(
        OUTPUT "${image_stamp}"
        COMMAND "${CMAKE_COMMAND}" "-DPODMAN=${IIDECK_PODMAN}" "-DIMAGE=${image}"
            "-DCONTAINERFILE=${IIDECK_GAMESCOPE_CONTAINERFILE}"
            "-DCONTEXT=${CMAKE_SOURCE_DIR}/packaging/gamescope-build"
            "-DFEDORA_VERSION=${fedora_version}" "-DSTAMP=${image_stamp}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GamescopeImage.cmake"
        DEPENDS "${IIDECK_GAMESCOPE_CONTAINERFILE}" "${CMAKE_SOURCE_DIR}/cmake/GamescopeImage.cmake"
        COMMENT "Gamescope build image ${image}"
        USES_TERMINAL VERBATIM)
    add_custom_target(iideck_gamescope_image DEPENDS "${image_stamp}")

    set(in_container "${IIDECK_PODMAN}" run --rm --userns=keep-id
        -v "${root}/source:${root}/source:Z" -v "${root}/build:${root}/build:Z"
        -e CC=clang -e CXX=clang++ "${image}")

    ExternalProject_Add(iideck_gamescope
        DEPENDS iideck_gamescope_image
        GIT_REPOSITORY "${IIDECK_GAMESCOPE_REPOSITORY}"
        GIT_TAG "${IIDECK_GAMESCOPE_COMMIT}"
        GIT_SUBMODULES_RECURSE ON
        GIT_PROGRESS ON
        UPDATE_DISCONNECTED ON
        SOURCE_DIR "${root}/source"
        BINARY_DIR "${root}/build"
        STAMP_DIR "${root}/stamp"
        TMP_DIR "${root}/tmp"
        DOWNLOAD_DIR "${root}/download"
        CONFIGURE_COMMAND ${in_container} meson setup "${root}/build" "${root}/source"
            --buildtype=release -Denable_gamescope_wsi_layer=false -Denable_tests=false
        BUILD_COMMAND ${in_container} ninja -C "${root}/build" src/gamescope
        BUILD_BYPRODUCTS "${built}"
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${built}" "${staged}"
        BUILD_ALWAYS OFF
        USES_TERMINAL_DOWNLOAD ON
        USES_TERMINAL_CONFIGURE ON
        USES_TERMINAL_BUILD ON
    )
    ExternalProject_Add_Step(iideck_gamescope check_runtime_libraries
        COMMAND "${CMAKE_COMMAND}" "-DBINARY=${staged}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GamescopeLddCheck.cmake"
        DEPENDEES install
        USES_TERMINAL ON)

    install(PROGRAMS "${staged}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/iideck")
endfunction()
