# The Gamescope fork the nested session runs.
#
# Stock Gamescope answers every close request from the host compositor with SIGTERM, so
# Alt+F4 in KDE ends the whole session. The fork adds --close-focused-window, which sends
# the close to the focused game instead. It is built from the pinned commit as an
# ExternalProject, with meson and ninja running inside a podman container built from
# packaging/gamescope-build/Containerfile, so the host needs no Gamescope build dependencies.
# The image is built once per Containerfile content, the fork once, and a second build does
# nothing. OPENSU_GAMESCOPE_BUILD_ENV=native runs meson and ninja directly instead, for a
# configure that already runs inside a build environment holding the dependencies (CI's Fedora
# job container, the AppImage build container).
#
# Layout: the binary is staged at <build>/<libexecdir>/opensu/gamescope, which is where the
# running build-tree executable looks (<exe dir>/../<libexecdir>/opensu/gamescope), and it is
# installed to <prefix>/<libexecdir>/opensu/gamescope, where an installed opensu looks.
#
# A pin change fetches the commit into the existing clone, checks it out, and reconfigures meson,
# which regenerates the version header from `git describe`; an unchanged pin runs none of these.

include(ExternalProject)
include(GNUInstallDirs)

option(OPENSU_BUILD_GAMESCOPE
    "Build the pinned Gamescope fork the nested session runs. OFF only for development of \
the rest of opensu: the nested session then refuses to start" ON)

set(OPENSU_GAMESCOPE_BUILD_ENV "podman" CACHE STRING
    "Where the fork's meson build runs: podman (a container matching the Fedora host) or native \
(this environment already has Gamescope's build dependencies)")
set_property(CACHE OPENSU_GAMESCOPE_BUILD_ENV PROPERTY STRINGS podman native)
set(OPENSU_GAMESCOPE_MESON_OPTIONS "" CACHE STRING
    "Extra meson options for the fork, for a build environment whose distribution lacks some of its \
dependencies (-Dforce_fallback_for=...)")

set(OPENSU_GAMESCOPE_REPOSITORY "https://github.com/SomeoneIsWorking/gamescope.git")
set(OPENSU_GAMESCOPE_COMMIT "c1a4a171a3a90802c26dc58ddc6e1eb9c4ecdd49")

# Where the running opensu finds the binary, relative to the directory above its own.
set(OPENSU_GAMESCOPE_RELATIVE "${CMAKE_INSTALL_LIBEXECDIR}/opensu/gamescope")

set(OPENSU_GAMESCOPE_CONTAINERFILE "${CMAKE_SOURCE_DIR}/packaging/gamescope-build/Containerfile")

# The image is based on the host's Fedora release, so the binary links the host's libraries.
function(opensu_gamescope_image_tag out fedora_version_out)
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
            "-DOPENSU_BUILD_GAMESCOPE=OFF to work on the rest of opensu; the nested session "
            "then refuses to start.")
    endif()
    file(SHA256 "${OPENSU_GAMESCOPE_CONTAINERFILE}" digest)
    string(SUBSTRING "${digest}" 0 12 short)
    set(${out} "localhost/opensu-gamescope-build:fedora-${version}-${short}" PARENT_SCOPE)
    set(${fedora_version_out} "${version}" PARENT_SCOPE)
endfunction()

# The meson and ninja command prefix and the target that prepares it, for the chosen build
# environment.
function(opensu_gamescope_runner root runner_out depends_out)
    if(OPENSU_GAMESCOPE_BUILD_ENV STREQUAL "native")
        foreach(tool meson ninja glslangValidator)
            find_program(OPENSU_GAMESCOPE_${tool} ${tool})
            if(NOT OPENSU_GAMESCOPE_${tool})
                message(FATAL_ERROR
                    "OPENSU_GAMESCOPE_BUILD_ENV=native needs ${tool} and Gamescope's build "
                    "dependencies in this environment (Fedora: `sudo dnf builddep gamescope`).")
            endif()
        endforeach()
        set(${runner_out} "${CMAKE_COMMAND}" -E env CC=clang CXX=clang++ PARENT_SCOPE)
        set(${depends_out} "" PARENT_SCOPE)
        return()
    endif()
    if(NOT OPENSU_GAMESCOPE_BUILD_ENV STREQUAL "podman")
        message(FATAL_ERROR "OPENSU_GAMESCOPE_BUILD_ENV must be podman or native, not "
            "'${OPENSU_GAMESCOPE_BUILD_ENV}'")
    endif()
    find_program(OPENSU_PODMAN podman)
    if(NOT OPENSU_PODMAN)
        message(FATAL_ERROR
            "Building the Gamescope fork needs podman (rootless). Install it: `sudo dnf install "
            "podman`. To work on the rest of opensu without the fork, configure with "
            "-DOPENSU_BUILD_GAMESCOPE=OFF.")
    endif()
    opensu_gamescope_image_tag(image fedora_version)
    set(image_stamp "${root}/image.stamp")

    add_custom_command(
        OUTPUT "${image_stamp}"
        COMMAND "${CMAKE_COMMAND}" "-DPODMAN=${OPENSU_PODMAN}" "-DIMAGE=${image}"
            "-DCONTAINERFILE=${OPENSU_GAMESCOPE_CONTAINERFILE}"
            "-DCONTEXT=${CMAKE_SOURCE_DIR}/packaging/gamescope-build"
            "-DFEDORA_VERSION=${fedora_version}" "-DSTAMP=${image_stamp}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GamescopeImage.cmake"
        DEPENDS "${OPENSU_GAMESCOPE_CONTAINERFILE}" "${CMAKE_SOURCE_DIR}/cmake/GamescopeImage.cmake"
        COMMENT "Gamescope build image ${image}"
        USES_TERMINAL VERBATIM)
    add_custom_target(opensu_gamescope_image DEPENDS "${image_stamp}")

    set(${runner_out} "${OPENSU_PODMAN}" run --rm --userns=keep-id
        -v "${root}/source:${root}/source:Z" -v "${root}/build:${root}/build:Z"
        -e CC=clang -e CXX=clang++ "${image}" PARENT_SCOPE)
    set(${depends_out} DEPENDS opensu_gamescope_image PARENT_SCOPE)
endfunction()

function(opensu_add_gamescope)
    find_package(Git REQUIRED)
    set(root "${CMAKE_BINARY_DIR}/gamescope")
    set(built "${root}/build/src/gamescope")
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_GAMESCOPE_RELATIVE}")
    set(pin_file "${root}/pin.txt")
    # Rewritten only when the pin changes, so it dates the pin.
    file(CONFIGURE OUTPUT "${pin_file}" CONTENT "${OPENSU_GAMESCOPE_COMMIT}\n")
    opensu_gamescope_runner("${root}" runner image_dependency)
    separate_arguments(extra_meson_options UNIX_COMMAND "${OPENSU_GAMESCOPE_MESON_OPTIONS}")

    ExternalProject_Add(opensu_gamescope
        ${image_dependency}
        GIT_REPOSITORY "${OPENSU_GAMESCOPE_REPOSITORY}"
        GIT_TAG "${OPENSU_GAMESCOPE_COMMIT}"
        GIT_SUBMODULES_RECURSE ON
        GIT_PROGRESS ON
        UPDATE_DISCONNECTED ON
        SOURCE_DIR "${root}/source"
        BINARY_DIR "${root}/build"
        STAMP_DIR "${root}/stamp"
        TMP_DIR "${root}/tmp"
        DOWNLOAD_DIR "${root}/download"
        CONFIGURE_COMMAND ${runner} meson setup --reconfigure "${root}/build" "${root}/source"
            --buildtype=release -Denable_gamescope_wsi_layer=false -Denable_tests=false
            ${extra_meson_options}
        BUILD_COMMAND ${runner} ninja -C "${root}/build" src/gamescope
        BUILD_BYPRODUCTS "${built}"
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${built}" "${staged}"
        BUILD_ALWAYS OFF
        USES_TERMINAL_DOWNLOAD ON
        USES_TERMINAL_CONFIGURE ON
        USES_TERMINAL_BUILD ON
    )
    # The disconnected update refuses a commit the clone does not have.
    ExternalProject_Add_Step(opensu_gamescope fetch_pin
        COMMAND "${CMAKE_COMMAND}" "-DGIT=${GIT_EXECUTABLE}" "-DSOURCE=${root}/source"
            "-DCOMMIT=${OPENSU_GAMESCOPE_COMMIT}" -DNAME=Gamescope
            -P "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDS "${pin_file}" "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDEES download
        DEPENDERS update_disconnected
        INDEPENDENT TRUE
        USES_TERMINAL ON)
    ExternalProject_Add_Step(opensu_gamescope check_runtime_libraries
        COMMAND "${CMAKE_COMMAND}" "-DBINARY=${staged}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GamescopeLddCheck.cmake"
        DEPENDEES install
        USES_TERMINAL ON)

    install(PROGRAMS "${staged}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/opensu")
endfunction()
