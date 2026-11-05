# The SDL fork (zlib), from its pinned commit: SDL 3.4.18 whose GPU API also wraps a DMA-BUF
# plane as a texture on Vulkan, with SDL_ttf and SDL_image (zlib, upstream releases) built against
# it, so every library linking SDL3 comes from one directory. Built shared, with every backend
# each finds in the build environment (tools.check's build image installs Fedora's build
# dependencies for them), relocatable, and installed under `opensu-sdl/` so nothing finds them by
# accident through CMAKE_PREFIX_PATH.

set(root "${CMAKE_BINARY_DIR}/sdl")
set(stage "${root}/prefix")

ExternalProject_Add(opensu_sdl
    GIT_REPOSITORY "${OPENSU_SDL_REPOSITORY}"
    GIT_TAG "${OPENSU_SDL_COMMIT}"
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/source"
    BINARY_DIR "${root}/build"
    INSTALL_DIR "${stage}"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_INSTALL_PREFIX=${stage}"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DSDL_SHARED=ON"
        "-DSDL_STATIC=OFF"
        "-DSDL_TEST_LIBRARY=OFF"
        "-DSDL_TESTS=OFF"
        "-DSDL_EXAMPLES=OFF"
        "-DSDL_RELOCATABLE=ON"
        "-DSDL_RPATH=OFF"
        "-DSDL_VENDOR_INFO=openSU fork ${OPENSU_SDL_COMMIT}"
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

set(satellite_args
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_INSTALL_PREFIX=${stage}"
    "-DCMAKE_INSTALL_LIBDIR=lib"
    "-DCMAKE_PREFIX_PATH=${stage}"
    "-DBUILD_SHARED_LIBS=ON"
)

ExternalProject_Add(opensu_sdl_ttf
    DEPENDS opensu_sdl
    GIT_REPOSITORY "${OPENSU_SDL_TTF_REPOSITORY}"
    GIT_TAG "${OPENSU_SDL_TTF_COMMIT}"
    GIT_SUBMODULES ""
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/ttf-source"
    BINARY_DIR "${root}/ttf-build"
    INSTALL_DIR "${stage}"
    CMAKE_ARGS ${satellite_args}
        "-DSDLTTF_VENDORED=OFF"
        "-DSDLTTF_STRICT=ON"
        "-DSDLTTF_HARFBUZZ=ON"
        "-DSDLTTF_PLUTOSVG=OFF"
        "-DSDLTTF_SAMPLES=OFF"
        "-DSDLTTF_RELOCATABLE=ON"
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

ExternalProject_Add(opensu_sdl_image
    DEPENDS opensu_sdl
    GIT_REPOSITORY "${OPENSU_SDL_IMAGE_REPOSITORY}"
    GIT_TAG "${OPENSU_SDL_IMAGE_COMMIT}"
    GIT_SUBMODULES ""
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/image-source"
    BINARY_DIR "${root}/image-build"
    INSTALL_DIR "${stage}"
    CMAKE_ARGS ${satellite_args}
        "-DSDLIMAGE_VENDORED=OFF"
        "-DSDLIMAGE_SAMPLES=OFF"
        "-DSDLIMAGE_TESTS=OFF"
        "-DSDLIMAGE_RELOCATABLE=ON"
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuSDLConfig.cmake" @ONLY CONTENT [=[
# The SDL fork, SDL_ttf and SDL_image from openSU's shared third-party prefix, at openSU's pins.
get_filename_component(_opensu_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpensuSDL_COMMIT "@OPENSU_SDL_COMMIT@")
set(OpensuSDL_PKGCONFIG_DIR "${_opensu_prefix}/opensu-sdl/lib/pkgconfig")
set(OpensuSDL_LIBRARY_DIR "${_opensu_prefix}/opensu-sdl/lib")
unset(_opensu_prefix)
]=])
install(DIRECTORY "${stage}/" DESTINATION opensu-sdl)
install(FILES "${root}/OpensuSDLConfig.cmake" DESTINATION lib/cmake/OpensuSDL)
