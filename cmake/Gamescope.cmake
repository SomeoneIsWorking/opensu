# The Gamescope fork the nested session runs.
#
# Stock Gamescope answers every close request from the host compositor with SIGTERM, so
# Alt+F4 in KDE ends the whole session. The fork adds --close-focused-window, which sends
# the close to the focused game instead. It is built from the pinned commit as an
# ExternalProject: configured and built once, and a second build compiles nothing.
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

# Hard build requirements of Gamescope's meson files, as pkg-config modules. The optional
# features (PipeWire, libei, AVIF, SDL2, libcap) are left to meson's auto detection.
set(IIDECK_GAMESCOPE_MODULES
    "x11" "xdamage" "xcomposite" "xcursor" "xrender" "xext" "xfixes" "xxf86vm" "xtst" "xres"
    "xmu" "xi" "libdrm >= 2.4.113" "wayland-server >= 1.21" "wayland-client"
    "wayland-protocols" "wayland-scanner" "xkbcommon" "pixman-1" "libudev" "libdecor-0"
    "luajit" "libinput >= 1.19" "vulkan")

# Gamescope uses the system wlroots 0.20 when there is one and otherwise builds its own
# subproject, which needs these.
set(IIDECK_GAMESCOPE_WLROOTS_MODULE "wlroots-0.20 >= 0.20.0")
set(IIDECK_GAMESCOPE_WLROOTS_SUBPROJECT_MODULES
    "libseat >= 0.2.0" "xwayland" "xcb" "xcb-composite" "xcb-ewmh" "xcb-icccm" "xcb-render"
    "xcb-res" "xcb-xfixes >= 1.15")

# Where the running iideck finds the binary, relative to the directory above its own.
set(IIDECK_GAMESCOPE_RELATIVE "${CMAKE_INSTALL_LIBEXECDIR}/iideck/gamescope")

function(iideck_require_gamescope_build_tools)
    find_program(IIDECK_MESON meson)
    find_program(IIDECK_NINJA ninja)
    find_program(IIDECK_GLSLANG NAMES glslang glslangValidator)
    set(missing "")
    foreach(tool MESON NINJA)
        if(NOT IIDECK_${tool})
            string(TOLOWER "${tool}" name)
            list(APPEND missing "${name}")
        endif()
    endforeach()
    if(missing)
        message(FATAL_ERROR
            "Building the Gamescope fork needs ${missing}. Without root: `uv tool install meson` "
            "(and `uv tool install ninja`), or the distribution's package.")
    endif()
endfunction()

function(iideck_missing_modules out)
    set(missing "")
    foreach(spec IN LISTS ARGN)
        string(REGEX REPLACE "[^A-Za-z0-9_]" "_" var "${spec}")
        pkg_check_modules(IIDECK_GS_${var} QUIET "${spec}")
        if(NOT IIDECK_GS_${var}_FOUND)
            list(APPEND missing "${spec}")
        endif()
    endforeach()
    set(${out} "${missing}" PARENT_SCOPE)
endfunction()

function(iideck_check_gamescope_dependencies)
    find_package(PkgConfig REQUIRED)
    iideck_missing_modules(missing ${IIDECK_GAMESCOPE_MODULES})
    iideck_missing_modules(no_wlroots "${IIDECK_GAMESCOPE_WLROOTS_MODULE}")
    if(no_wlroots)
        iideck_missing_modules(missing_wlroots ${IIDECK_GAMESCOPE_WLROOTS_SUBPROJECT_MODULES})
        list(APPEND missing ${missing_wlroots})
    endif()
    if(NOT IIDECK_GLSLANG)
        list(APPEND missing "glslang (glslang or glslangValidator)")
    endif()
    if(missing)
        list(JOIN missing ", " names)
        message(FATAL_ERROR
            "The Gamescope fork cannot be built: missing build dependencies: ${names}.\n"
            "Install them with the distribution's Gamescope build dependencies:\n"
            "  Fedora:        sudo dnf builddep gamescope\n"
            "  Debian/Ubuntu: sudo apt build-dep gamescope\n"
            "  Arch:          sudo pacman -S --needed benchmark cmake glslang meson ninja "
            "vulkan-headers wayland-protocols lcms2 libavif libcap libdecor libdrm libei "
            "libinput pipewire libx11 libxcb libxcomposite libxcursor libxdamage libxext "
            "libxfixes libxi libxkbcommon libxmu libxrender libxres libxtst libxxf86vm luajit "
            "pixman sdl2 seatd systemd-libs vulkan-icd-loader wayland xcb-util-errors "
            "xcb-util-wm xorg-server-xwayland\n"
            "To work on the rest of iideck without the fork, configure with "
            "-DIIDECK_BUILD_GAMESCOPE=OFF; the nested session then refuses to start.")
    endif()
endfunction()

function(iideck_add_gamescope)
    iideck_require_gamescope_build_tools()
    iideck_check_gamescope_dependencies()

    set(root "${CMAKE_BINARY_DIR}/gamescope")
    set(built "${root}/build/src/gamescope")
    set(staged "${CMAKE_BINARY_DIR}/${IIDECK_GAMESCOPE_RELATIVE}")
    set(meson_env "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}")

    ExternalProject_Add(iideck_gamescope
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
        CONFIGURE_COMMAND ${meson_env} "${IIDECK_MESON}" setup "${root}/build" "${root}/source"
            --buildtype=release -Denable_gamescope_wsi_layer=false -Denable_tests=false
        BUILD_COMMAND "${IIDECK_NINJA}" -C "${root}/build" src/gamescope
        BUILD_BYPRODUCTS "${built}"
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${built}" "${staged}"
        BUILD_ALWAYS OFF
        USES_TERMINAL_DOWNLOAD ON
        USES_TERMINAL_CONFIGURE ON
        USES_TERMINAL_BUILD ON
    )

    install(PROGRAMS "${staged}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/iideck")
endfunction()
