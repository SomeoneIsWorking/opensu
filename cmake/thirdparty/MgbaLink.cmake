# The mGBA fork with link-cable multiplayer (SomeoneIsWorking/mgba, libretro/mgba PR #318), the
# built-in runner's core for linked GBA games: its libretro core built with mGBA's own CMake as
# libretro's CI does, installed as `mgba_link_libretro` so it never stands in for the buildbot's
# `mgba_libretro`. MPL-2.0.

set(root "${CMAKE_BINARY_DIR}/mgba-link")
set(built_name "mgba_libretro${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(library "mgba_link_libretro${CMAKE_SHARED_LIBRARY_SUFFIX}")

ExternalProject_Add(opensu_mgba_link
    GIT_REPOSITORY "${OPENSU_MGBA_LINK_REPOSITORY}"
    GIT_TAG "${OPENSU_MGBA_LINK_COMMIT}"
    GIT_SUBMODULES ""
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/source"
    BINARY_DIR "${root}/build"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DLIBMGBA_ONLY=ON"
        "-DBUILD_LIBRETRO=ON"
    BUILD_COMMAND "${CMAKE_COMMAND}" --build "${root}/build" --target mgba_libretro
    BUILD_BYPRODUCTS "${root}/build/${built_name}"
    INSTALL_COMMAND ""
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuMgbaLinkConfig.cmake" @ONLY CONTENT [=[
# The mGBA link core from openSU's shared third-party prefix, at the commit openSU pins.
get_filename_component(_opensu_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpensuMgbaLink_COMMIT "@OPENSU_MGBA_LINK_COMMIT@")
set(OpensuMgbaLink_LIBRARY "${_opensu_prefix}/lib/@library@")
unset(_opensu_prefix)
]=])
install(FILES "${root}/build/${built_name}" DESTINATION lib RENAME "${library}")
install(FILES "${root}/OpensuMgbaLinkConfig.cmake" DESTINATION lib/cmake/OpensuMgbaLink)
