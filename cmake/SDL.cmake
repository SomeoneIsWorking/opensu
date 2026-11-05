# SDL3 for the shell and the runner: on Linux the pinned fork from the shared third-party prefix
# (cmake/thirdparty/SDL.cmake, built by `tools.check deps`), whose GPU API wraps a DMA-BUF plane as
# a texture, so remote play's guest draws the hardware decoder's picture where it lies; elsewhere,
# or with OPENSU_BUILD_SDL_FORK=OFF, the system's, and the guest reads each picture back. The fork
# and the SDL_ttf and SDL_image built against it are put first on pkg-config's path, so gpu2d's
# and openSU's lookups find them. The libraries are installed in openSU's own library directory,
# which the executables' install RPATH names; a build-tree executable loads them from the prefix.

include(GNUInstallDirs)
include(ThirdParty)

option(OPENSU_BUILD_SDL_FORK
    "Use the pinned SDL fork, whose GPU API imports a decoded video picture. OFF uses the system's \
SDL3, and remote play's guest copies each hardware-decoded picture through the CPU" ON)

if(OPENSU_BUILD_SDL_FORK AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    opensu_require_thirdparty(OpensuSDL "${OPENSU_SDL_COMMIT}" OPENSU_BUILD_SDL_FORK)
    set(ENV{PKG_CONFIG_PATH} "${OpensuSDL_PKGCONFIG_DIR}:$ENV{PKG_CONFIG_PATH}")
    set(OPENSU_SDL_DIR "${CMAKE_INSTALL_LIBDIR}/opensu")
    list(APPEND CMAKE_INSTALL_RPATH "\$ORIGIN/../${OPENSU_SDL_DIR}")
    # Each soname is a link to the versioned file; the file goes in under the soname.
    foreach(name IN ITEMS SDL3 SDL3_ttf SDL3_image)
        set(soname "${CMAKE_SHARED_LIBRARY_PREFIX}${name}${CMAKE_SHARED_LIBRARY_SUFFIX}.0")
        file(REAL_PATH "${OpensuSDL_LIBRARY_DIR}/${soname}" library)
        install(FILES "${library}" DESTINATION "${OPENSU_SDL_DIR}" RENAME "${soname}")
    endforeach()
endif()

# pkg_check_modules keeps its answers in the cache; one found on another search path is stale.
if(NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "${OPENSU_PKG_CONFIG_PATH_SEEN}")
    get_cmake_property(cached CACHE_VARIABLES)
    list(FILTER cached INCLUDE REGEX "^(__pkg_config_checked_|pkgcfg_lib_)")
    foreach(variable IN LISTS cached)
        unset(${variable} CACHE)
    endforeach()
    set(OPENSU_PKG_CONFIG_PATH_SEEN "$ENV{PKG_CONFIG_PATH}" CACHE INTERNAL
        "The pkg-config search path the cached lookups were made on")
endif()
