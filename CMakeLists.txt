cmake_minimum_required(VERSION 3.28)
project(opensu LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(MissingPackage)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# A macOS install is the inside of openSU.app, its prefix the bundle's Contents: the executables in
# MacOS, the assets in Resources, librashader and the Vulkan loader in Frameworks. Set before any
# project includes GNUInstallDirs, in the build tree as installed.
if(APPLE)
    set(CMAKE_INSTALL_BINDIR "MacOS" CACHE PATH "Executables in openSU.app/Contents")
    set(CMAKE_INSTALL_DATADIR "Resources" CACHE PATH "Assets in openSU.app/Contents")
    set(CMAKE_INSTALL_LIBDIR "Frameworks" CACHE PATH "Libraries in openSU.app/Contents")
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
endif()

# nanosvg rasterises the shipped icons: the distribution's package or a checkout.
set(NANOSVG_ROOT "$ENV{HOME}/dev/nanosvg" CACHE PATH "nanosvg checkout")
find_path(NANOSVG_INCLUDE_DIR nanosvg.h
    PATH_SUFFIXES nanosvg PATHS "${NANOSVG_ROOT}/src")
if(NOT NANOSVG_INCLUDE_DIR)
    opensu_missing_package("openSU needs nanosvg to draw its icons"
        FEDORA nanosvg-devel DEBIAN libnanosvg-dev
        OTHERWISE "clone https://github.com/memononen/nanosvg to ${NANOSVG_ROOT}")
endif()
# Its implementation is compiled from a file in the build tree, so openSU's lint, which covers src/
# and tests/, does not analyse nanosvg's code.
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/nanosvg/nanosvg_implementation.c" CONTENT
    "#define NANOSVG_IMPLEMENTATION\n#include <nanosvg.h>\n#define NANOSVGRAST_IMPLEMENTATION\n#include <nanosvgrast.h>\n")
add_library(opensu_nanosvg STATIC "${CMAKE_BINARY_DIR}/nanosvg/nanosvg_implementation.c")
add_library(opensu::nanosvg ALIAS opensu_nanosvg)
target_include_directories(opensu_nanosvg SYSTEM PUBLIC "${NANOSVG_INCLUDE_DIR}")
if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(opensu_nanosvg PRIVATE m)
endif()

# zlib inflates the zips opensu reads.
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)

# SDL3 gives the shell its window, input and sound and, with Vulkan, draws opensu-retro;
# libvorbisfile decodes the UI sounds.
include(SDL)
pkg_check_modules(SDL3 IMPORTED_TARGET GLOBAL sdl3>=3.2)
pkg_check_modules(VORBISFILE IMPORTED_TARGET GLOBAL vorbisfile)
# The Vulkan loader: pkg-config's on the desktop, the NDK sysroot's on Android.
add_library(opensu_vulkan INTERFACE)
add_library(opensu::vulkan ALIAS opensu_vulkan)
if(ANDROID)
    find_package(Vulkan REQUIRED)
    target_link_libraries(opensu_vulkan INTERFACE Vulkan::Vulkan)
    set(VULKAN_FOUND TRUE)
else()
    pkg_check_modules(VULKAN IMPORTED_TARGET GLOBAL vulkan)
    if(VULKAN_FOUND)
        target_link_libraries(opensu_vulkan INTERFACE PkgConfig::VULKAN)
    endif()
endif()
if(NOT SDL3_FOUND OR NOT VULKAN_FOUND OR NOT VORBISFILE_FOUND)
    opensu_missing_package("openSU needs SDL3, the Vulkan loader and libvorbisfile"
        OTHERWISE "on Ubuntu 24.04, which packages no SDL3, build it as packaging/appimage/Containerfile does"
        FEDORA "SDL3-devel vulkan-loader-devel libvorbis-devel"
        DEBIAN "libvulkan-dev libvorbis-dev")
endif()

# libcrypto decrypts the Switch game files whose own title and icon a Switch tile shows.
find_package(OpenSSL 3.0 COMPONENTS Crypto)
if(NOT OPENSSL_FOUND)
    opensu_missing_package("openSU needs libcrypto (OpenSSL 3)"
        FEDORA openssl-devel DEBIAN libssl-dev)
endif()

# nlohmann/json parses and writes every JSON file and reply openSU handles.
find_package(nlohmann_json 3 QUIET)
if(NOT nlohmann_json_FOUND)
    opensu_missing_package("openSU needs nlohmann/json 3"
        FEDORA json-devel DEBIAN nlohmann-json3-dev)
endif()

# SQLite reads Lutris's game list (`library/lutris_games`) for Add a game's import.
pkg_check_modules(SQLITE3 IMPORTED_TARGET GLOBAL sqlite3)
if(NOT SQLITE3_FOUND)
    opensu_missing_package("openSU needs SQLite 3"
        FEDORA sqlite-devel DEBIAN libsqlite3-dev)
endif()

# Lucent is the project logger. Shared, not vendored.
set(LUCENT_ROOT "$ENV{HOME}/repo/lucent" CACHE PATH "Shared Lucent checkout")

add_subdirectory("${LUCENT_ROOT}" "${CMAKE_BINARY_DIR}/lucent" EXCLUDE_FROM_ALL)

# gpu2d draws the shell on SDL3 GPU and decodes, resizes and encodes images. Shared, not vendored;
# it also provides gpu2d_compile_shaders.
set(GPU2D_ROOT "$ENV{HOME}/repo/gpu2d" CACHE PATH "Shared gpu2d checkout")

add_subdirectory("${GPU2D_ROOT}" "${CMAKE_BINARY_DIR}/gpu2d" EXCLUDE_FROM_ALL)

# The Gamescope fork the nested session runs; the running binary looks for it by the
# relative path below, in the build tree and installed alike. Gamescope, the session and X11 are
# Linux's alone.
include(GNUInstallDirs)
include(Gamescope)
include(Lua)
include(Libretro)
include(Librashader)
include(MgbaLink)
include(PyroWave)
include(Warnings)
include(Windowed)
if(OPENSU_BUILD_GAMESCOPE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    opensu_add_gamescope()
endif()

add_subdirectory(src)
add_subdirectory(extension)
if(TARGET opensu_gamescope)
    add_dependencies(opensu opensu_gamescope)
endif()

enable_testing()
add_subdirectory(tests)