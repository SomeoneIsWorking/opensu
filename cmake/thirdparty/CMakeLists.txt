# The third-party code openSU builds against or ships, each from its pinned commit, installed into
# one prefix outside every openSU build tree: PyroWave with Granite, librashader, Opus (Android),
# the Gamescope fork (Linux) and the SDL fork (Linux). tools/check
# (`thirdparty.py`) configures this with the compilers or toolchain an openSU tree uses and installs
# it under a directory named for those, the build image and these files, so each revision,
# toolchain and image is built once. Each installs a CMake package:
# `find_package(OpensuPyroWave)`, `OpensuLibrashader`, `OpensuOpus`, `OpensuGamescope` and
# `OpensuSDL`.
cmake_minimum_required(VERSION 3.28)
project(opensu_thirdparty LANGUAGES C CXX)

include(ExternalProject)
include("${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty.cmake")
find_package(Git REQUIRED)

include("${CMAKE_CURRENT_SOURCE_DIR}/PyroWave.cmake")
if(ANDROID)
    include("${CMAKE_CURRENT_SOURCE_DIR}/Opus.cmake")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/Gamescope.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/SDL.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/Librashader.cmake")
