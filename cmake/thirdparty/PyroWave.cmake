# PyroWave, remote play's video codec (Vulkan compute wavelets, intra-only): the C API of the pinned
# fork, `libpyrowave-shared`, built with CMake against the part of Granite it needs (volk and the
# Vulkan headers), at the Granite commit PyroWave's own checkout script pins. Both are MIT. It is
# built with this prefix's own compilers: the Windows toolchain file, macOS's SDK and target.

set(root "${CMAKE_BINARY_DIR}/pyrowave")
set(stage "${root}/prefix")
# The file the loader finds at run time, and on Windows the import library openSU links.
if(WIN32)
    set(runtime "bin/libpyrowave-shared-1.dll")
    set(import_library "lib/pyrowave-shared.lib")
elseif(APPLE)
    set(runtime "lib/libpyrowave-shared.1.dylib")
    set(import_library "")
else()
    set(runtime "lib/libpyrowave-shared.so.1")
    set(import_library "")
endif()

set(forwarded "")
foreach(variable IN ITEMS CMAKE_TOOLCHAIN_FILE CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
                          CMAKE_OSX_SYSROOT)
    if(DEFINED ${variable})
        list(APPEND forwarded "-D${variable}=${${variable}}")
    endif()
endforeach()

ExternalProject_Add(opensu_granite_source
    GIT_REPOSITORY "${OPENSU_GRANITE_REPOSITORY}"
    GIT_TAG "${OPENSU_GRANITE_COMMIT}"
    GIT_SUBMODULES "third_party/volk;third_party/khronos/vulkan-headers"
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/granite"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    USES_TERMINAL_DOWNLOAD ON
)
ExternalProject_Add(opensu_pyrowave
    GIT_REPOSITORY "${OPENSU_PYROWAVE_REPOSITORY}"
    GIT_TAG "${OPENSU_PYROWAVE_COMMIT}"
    GIT_SUBMODULES ""
    GIT_PROGRESS ON
    DEPENDS opensu_granite_source
    SOURCE_DIR "${root}/source"
    BINARY_DIR "${root}/build"
    INSTALL_DIR "${stage}"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        ${forwarded}
        "-DCMAKE_INSTALL_PREFIX=${stage}"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DPYROWAVE_GRANITE_DIR=${root}/granite"
        "-DPYROWAVE_TESTS=OFF"
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuPyroWaveConfig.cmake" @ONLY CONTENT [=[
# PyroWave from openSU's shared third-party prefix, at the commit openSU pins.
get_filename_component(_opensu_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpensuPyroWave_COMMIT "@OPENSU_PYROWAVE_COMMIT@")
set(OpensuPyroWave_LIBRARY "${_opensu_prefix}/@runtime@")
get_filename_component(OpensuPyroWave_SONAME "@runtime@" NAME)
if("@import_library@" STREQUAL "")
    set(OpensuPyroWave_IMPORT_LIBRARY "")
else()
    set(OpensuPyroWave_IMPORT_LIBRARY "${_opensu_prefix}/@import_library@")
endif()
set(OpensuPyroWave_INCLUDE_DIR "${_opensu_prefix}/include")
unset(_opensu_prefix)
]=])
# Its pkg-config file names the stage; openSU reads the CMake package below.
install(DIRECTORY "${stage}/lib/" DESTINATION lib
    FILES_MATCHING PATTERN "*pyrowave-shared*" PATTERN "pkgconfig" EXCLUDE)
if(WIN32)
    install(DIRECTORY "${stage}/bin/" DESTINATION bin FILES_MATCHING PATTERN "*pyrowave-shared*")
endif()
install(DIRECTORY "${stage}/include/" DESTINATION include)
install(FILES "${root}/OpensuPyroWaveConfig.cmake" DESTINATION lib/cmake/OpensuPyroWave)
