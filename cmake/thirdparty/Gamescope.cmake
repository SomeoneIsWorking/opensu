# The Gamescope fork the nested session runs (BSD-2-Clause), from its pinned commit: meson and
# ninja run in the build environment that holds its dependencies (tools.check's build image, the
# AppImage's container), and only `src/gamescope` is built, installed at `libexec/opensu/gamescope`.
#
# OPENSU_GAMESCOPE_MESON_OPTIONS adds meson options for a distribution that lacks some of its
# dependencies (-Dforce_fallback_for=...); OPENSU_GAMESCOPE_JOBS caps the fork's ninja, which the
# outer build's job count does not reach (0: ninja's default).

set(OPENSU_GAMESCOPE_MESON_OPTIONS "" CACHE STRING "Extra meson options for the Gamescope fork")
set(OPENSU_GAMESCOPE_JOBS 0 CACHE STRING "Compilers the Gamescope fork's ninja runs at once")

foreach(tool meson ninja glslangValidator)
    find_program(OPENSU_GAMESCOPE_${tool} ${tool})
    if(NOT OPENSU_GAMESCOPE_${tool})
        message(FATAL_ERROR "The Gamescope fork needs ${tool} and Gamescope's build dependencies "
            "(Fedora: `sudo dnf builddep gamescope`); tools.check's build image has them.")
    endif()
endforeach()

set(root "${CMAKE_BINARY_DIR}/gamescope")
set(built "${root}/build/src/gamescope")
separate_arguments(extra_meson_options UNIX_COMMAND "${OPENSU_GAMESCOPE_MESON_OPTIONS}")
set(jobs "")
if(OPENSU_GAMESCOPE_JOBS GREATER 0)
    set(jobs -j "${OPENSU_GAMESCOPE_JOBS}")
endif()
set(compilers "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}")

ExternalProject_Add(opensu_gamescope
    GIT_REPOSITORY "${OPENSU_GAMESCOPE_REPOSITORY}"
    GIT_TAG "${OPENSU_GAMESCOPE_COMMIT}"
    GIT_SUBMODULES_RECURSE ON
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/source"
    BINARY_DIR "${root}/build"
    CONFIGURE_COMMAND ${compilers} "${OPENSU_GAMESCOPE_meson}" setup "${root}/build"
        "${root}/source" --buildtype=release -Denable_gamescope_wsi_layer=false
        -Denable_tests=false ${extra_meson_options}
    BUILD_COMMAND ${compilers} "${OPENSU_GAMESCOPE_ninja}" -C "${root}/build" ${jobs} src/gamescope
    BUILD_BYPRODUCTS "${built}"
    INSTALL_COMMAND ""
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuGamescopeConfig.cmake" @ONLY CONTENT [=[
# The Gamescope fork from openSU's shared third-party prefix, at the commit openSU pins.
get_filename_component(_opensu_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpensuGamescope_COMMIT "@OPENSU_GAMESCOPE_COMMIT@")
set(OpensuGamescope_EXECUTABLE "${_opensu_prefix}/libexec/opensu/gamescope")
unset(_opensu_prefix)
]=])
install(PROGRAMS "${built}" DESTINATION libexec/opensu)
install(FILES "${root}/OpensuGamescopeConfig.cmake" DESTINATION lib/cmake/OpensuGamescope)
