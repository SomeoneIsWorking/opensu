cmake_minimum_required(VERSION 3.28)
project(iideck LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# raylib ships its headers and static library in a prefix. RAYLIB_ROOT points at
# that prefix; the README says why the published prebuilt binary cannot be used.
set(RAYLIB_ROOT "$ENV{HOME}/dev/raylib-built" CACHE PATH "Unpacked raylib prefix")
if(NOT EXISTS "${RAYLIB_ROOT}/include/raylib.h")
    message(FATAL_ERROR
        "raylib not found under ${RAYLIB_ROOT}. Set RAYLIB_ROOT to a prefix "
        "containing include/raylib.h and lib/libraylib.a")
endif()

# raylib's image decode path is compiled out unless these are defined, and the
# archive alone is not enough: consumers need the same flags.
set(RAYLIB_IMAGE_FLAGS STBI_REQUIRED SUPPORT_FILEFORMAT_JPG=1 SUPPORT_FILEFORMAT_TGA=1
                      SUPPORT_FILEFORMAT_PSD=1)
add_compile_definitions(${RAYLIB_IMAGE_FLAGS})

# The one raylib dependency: headers, the static archive and the platform libraries it leaves to
# its consumer.
find_package(X11 REQUIRED)
add_library(iideck_raylib INTERFACE)
add_library(iideck::raylib ALIAS iideck_raylib)
target_include_directories(iideck_raylib INTERFACE "${RAYLIB_ROOT}/include")
target_link_directories(iideck_raylib INTERFACE "${RAYLIB_ROOT}/lib")
target_link_libraries(iideck_raylib
    INTERFACE raylib X11::X11 GL m Threads::Threads ${CMAKE_DL_LIBS})

# nanosvg rasterises the shipped icons. Header-only: the distribution's package or a checkout.
set(NANOSVG_ROOT "$ENV{HOME}/dev/nanosvg" CACHE PATH "nanosvg checkout")
find_path(NANOSVG_INCLUDE_DIR nanosvg.h
    HINTS "${NANOSVG_ROOT}/src" PATH_SUFFIXES nanosvg)
if(NOT NANOSVG_INCLUDE_DIR)
    message(FATAL_ERROR
        "nanosvg not found. Install it with `sudo dnf install nanosvg-devel` (Fedora), "
        "`sudo apt install libnanosvg-dev` (Debian/Ubuntu), or clone "
        "https://github.com/memononen/nanosvg to ${NANOSVG_ROOT}")
endif()
add_library(iideck_nanosvg INTERFACE)
add_library(iideck::nanosvg ALIAS iideck_nanosvg)
target_include_directories(iideck_nanosvg SYSTEM INTERFACE "${NANOSVG_INCLUDE_DIR}")

# Lucent is the project logger. Shared, not vendored.
set(LUCENT_ROOT "$ENV{HOME}/repo/lucent" CACHE PATH "Shared Lucent checkout")

add_subdirectory("${LUCENT_ROOT}" "${CMAKE_BINARY_DIR}/lucent" EXCLUDE_FROM_ALL)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)