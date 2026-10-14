cmake_minimum_required(VERSION 3.28)
project(opensu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# The session reads X11 input and window properties.
find_package(X11 REQUIRED)

# nanosvg rasterises the shipped icons. Header-only: the distribution's package or a checkout.
set(NANOSVG_ROOT "$ENV{HOME}/dev/nanosvg" CACHE PATH "nanosvg checkout")
find_path(NANOSVG_INCLUDE_DIR nanosvg.h
    PATH_SUFFIXES nanosvg PATHS "${NANOSVG_ROOT}/src")
if(NOT NANOSVG_INCLUDE_DIR)
    message(FATAL_ERROR
        "nanosvg not found. Install it with `sudo dnf install nanosvg-devel` (Fedora), "
        "`sudo apt install libnanosvg-dev` (Debian/Ubuntu), or clone "
        "https://github.com/memononen/nanosvg to ${NANOSVG_ROOT}")
endif()
add_library(opensu_nanosvg INTERFACE)
add_library(opensu::nanosvg ALIAS opensu_nanosvg)
target_include_directories(opensu_nanosvg SYSTEM INTERFACE "${NANOSVG_INCLUDE_DIR}")

# The starter pack is a zip of WebP images: zlib reads the zip, SDL3_image the images (libwebp
# encodes the tests' fixture).
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET GLOBAL libwebp)

# SDL3 gives the shell its window, input and sound and, with Vulkan, draws opensu-retro;
# libvorbisfile decodes the UI sounds.
pkg_check_modules(SDL3 IMPORTED_TARGET GLOBAL sdl3>=3.2)
pkg_check_modules(VULKAN IMPORTED_TARGET GLOBAL vulkan)
pkg_check_modules(VORBISFILE IMPORTED_TARGET GLOBAL vorbisfile)
if(NOT SDL3_FOUND OR NOT VULKAN_FOUND OR NOT VORBISFILE_FOUND)
    message(FATAL_ERROR
        "openSU needs SDL3, the Vulkan loader and libvorbisfile. Install them with "
        "`sudo dnf install SDL3-devel vulkan-loader-devel libvorbis-devel` (Fedora); Ubuntu 24.04 "
        "packages no SDL3, so build it as packaging/appimage/Containerfile does and "
        "`sudo apt install libvulkan-dev libvorbis-dev`")
endif()

# Lucent is the project logger. Shared, not vendored.
set(LUCENT_ROOT "$ENV{HOME}/repo/lucent" CACHE PATH "Shared Lucent checkout")

add_subdirectory("${LUCENT_ROOT}" "${CMAKE_BINARY_DIR}/lucent" EXCLUDE_FROM_ALL)

# gpu2d draws the shell on SDL3 GPU and decodes, resizes and encodes images. Shared, not vendored;
# it also provides gpu2d_compile_shaders.
set(GPU2D_ROOT "$ENV{HOME}/repo/gpu2d" CACHE PATH "Shared gpu2d checkout")

add_subdirectory("${GPU2D_ROOT}" "${CMAKE_BINARY_DIR}/gpu2d" EXCLUDE_FROM_ALL)

# The Gamescope fork the nested session runs; the running binary looks for it by the
# relative path below, in the build tree and installed alike.
include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(Gamescope)
include(Lua)
include(Libretro)
if(OPENSU_BUILD_GAMESCOPE)
    opensu_add_gamescope()
endif()

add_subdirectory(src)
add_subdirectory(extension)
if(OPENSU_BUILD_GAMESCOPE)
    add_dependencies(opensu opensu_gamescope)
endif()

enable_testing()
add_subdirectory(tests)