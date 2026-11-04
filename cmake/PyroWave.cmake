# PyroWave, remote play's video codec (Vulkan compute wavelets, intra-only): the C API of the
# pinned fork, `libpyrowave-shared`, built with CMake as an ExternalProject against the part of
# Granite it needs (volk and the Vulkan headers), at the Granite commit PyroWave's own checkout
# script pins. Both are MIT. The library is staged beside librashader in the build tree and
# installed there, `<prefix>/<libdir>/opensu/`, which openSU's RPATH names.
#
# OPENSU_BUILD_PYROWAVE=OFF builds without it: remote play then refuses to start with the reason.
# Built on Linux, the one system whose host side (Gamescope's PipeWire stream) exists. A pin change
# fetches the commit into the existing clone; an unchanged pin runs nothing.

include(ExternalProject)
include(GNUInstallDirs)

option(OPENSU_BUILD_PYROWAVE
    "Build the pinned PyroWave fork remote play encodes and decodes video with. OFF builds \
openSU without remote play's video" ON)

set(OPENSU_PYROWAVE_REPOSITORY "https://github.com/SomeoneIsWorking/pyrowave.git")
set(OPENSU_PYROWAVE_COMMIT "8d19bce43f4fd97c7af3c40f2713ecd9db974642")
set(OPENSU_GRANITE_REPOSITORY "https://github.com/Themaister/Granite.git")
set(OPENSU_GRANITE_COMMIT "53090429539bf3404250d18fe58db971f0907c04")
set(OPENSU_PYROWAVE_DIR "${CMAKE_INSTALL_LIBDIR}/opensu")

function(opensu_add_pyrowave)
    find_package(Git REQUIRED)
    set(root "${CMAKE_BINARY_DIR}/pyrowave")
    set(prefix "${root}/prefix")
    set(library "${CMAKE_SHARED_LIBRARY_PREFIX}pyrowave-shared${CMAKE_SHARED_LIBRARY_SUFFIX}.1")
    set(built "${prefix}/lib/${library}")
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_PYROWAVE_DIR}/${library}")
    file(CONFIGURE OUTPUT "${root}/granite-pin.txt" CONTENT "${OPENSU_GRANITE_COMMIT}\n")
    file(CONFIGURE OUTPUT "${root}/pin.txt" CONTENT "${OPENSU_PYROWAVE_COMMIT}\n")

    ExternalProject_Add(opensu_granite_source
        GIT_REPOSITORY "${OPENSU_GRANITE_REPOSITORY}"
        GIT_TAG "${OPENSU_GRANITE_COMMIT}"
        GIT_SUBMODULES "third_party/volk;third_party/khronos/vulkan-headers"
        GIT_PROGRESS ON
        UPDATE_DISCONNECTED ON
        SOURCE_DIR "${root}/granite"
        STAMP_DIR "${root}/granite-stamp"
        TMP_DIR "${root}/tmp"
        DOWNLOAD_DIR "${root}/download"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        USES_TERMINAL_DOWNLOAD ON
    )
    ExternalProject_Add_Step(opensu_granite_source fetch_pin
        COMMAND "${CMAKE_COMMAND}" "-DGIT=${GIT_EXECUTABLE}" "-DSOURCE=${root}/granite"
            "-DCOMMIT=${OPENSU_GRANITE_COMMIT}" -DNAME=Granite
            -P "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDS "${root}/granite-pin.txt" "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDEES download
        DEPENDERS update_disconnected
        INDEPENDENT TRUE
        USES_TERMINAL ON)

    ExternalProject_Add(opensu_pyrowave
        GIT_REPOSITORY "${OPENSU_PYROWAVE_REPOSITORY}"
        GIT_TAG "${OPENSU_PYROWAVE_COMMIT}"
        GIT_SUBMODULES ""
        GIT_PROGRESS ON
        UPDATE_DISCONNECTED ON
        DEPENDS opensu_granite_source
        SOURCE_DIR "${root}/source"
        BINARY_DIR "${root}/build"
        STAMP_DIR "${root}/stamp"
        TMP_DIR "${root}/tmp"
        DOWNLOAD_DIR "${root}/download"
        INSTALL_DIR "${prefix}"
        CMAKE_ARGS
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_INSTALL_PREFIX=${prefix}"
            "-DCMAKE_INSTALL_LIBDIR=lib"
            "-DPYROWAVE_GRANITE_DIR=${root}/granite"
            "-DPYROWAVE_TESTS=OFF"
        BUILD_BYPRODUCTS "${built}"
        BUILD_ALWAYS OFF
        USES_TERMINAL_DOWNLOAD ON
        USES_TERMINAL_BUILD ON
    )
    ExternalProject_Add_Step(opensu_pyrowave fetch_pin
        COMMAND "${CMAKE_COMMAND}" "-DGIT=${GIT_EXECUTABLE}" "-DSOURCE=${root}/source"
            "-DCOMMIT=${OPENSU_PYROWAVE_COMMIT}" -DNAME=PyroWave
            -P "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDS "${root}/pin.txt" "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDEES download
        DEPENDERS update_disconnected
        INDEPENDENT TRUE
        USES_TERMINAL ON)
    ExternalProject_Add_Step(opensu_pyrowave stage
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${built}" "${staged}"
        DEPENDEES install
        BYPRODUCTS "${staged}")

    # The header is installed by the build; the directory exists before it for the imported target.
    file(MAKE_DIRECTORY "${prefix}/include")
    add_library(opensu_pyrowave_library SHARED IMPORTED GLOBAL)
    add_library(opensu::pyrowave ALIAS opensu_pyrowave_library)
    set_target_properties(opensu_pyrowave_library PROPERTIES
        IMPORTED_LOCATION "${staged}"
        IMPORTED_SONAME "${library}"
        INTERFACE_INCLUDE_DIRECTORIES "${prefix}/include")

    install(FILES "${staged}" DESTINATION "${OPENSU_PYROWAVE_DIR}")
endfunction()

if(OPENSU_BUILD_PYROWAVE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    opensu_add_pyrowave()
endif()
