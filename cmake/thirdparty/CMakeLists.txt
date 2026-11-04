# The third-party libraries openSU builds against, each from its pinned commit, installed into one
# prefix outside every openSU build tree: PyroWave with Granite (Linux) and librashader. tools/check
# (`thirdparty.py`) configures this with the compilers or toolchain an openSU tree uses and installs
# it under a directory named for those and for these files, so each revision and toolchain is built
# once. Each library installs a CMake package, `find_package(OpensuPyroWave)` and
# `find_package(OpensuLibrashader)`.
cmake_minimum_required(VERSION 3.28)
project(opensu_thirdparty LANGUAGES C CXX)

include(ExternalProject)
include("${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty.cmake")
find_package(Git REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/PyroWave.cmake")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/Librashader.cmake")
