# The third-party code openSU builds against or ships through the shared prefix (cmake/thirdparty):
# the pinned commits, which that build reads too, and finding them there.

set(OPENSU_PYROWAVE_REPOSITORY "https://github.com/SomeoneIsWorking/pyrowave.git")
set(OPENSU_PYROWAVE_COMMIT "92014b63d5db0e15dca4fb1e301e0b784cd290da")
set(OPENSU_GRANITE_REPOSITORY "https://github.com/SomeoneIsWorking/Granite.git")
set(OPENSU_GRANITE_COMMIT "192e75cd370a65a0d5a914df3961c9901d4d41ed")
set(OPENSU_LIBRASHADER_REPOSITORY "https://github.com/SomeoneIsWorking/librashader.git")
set(OPENSU_LIBRASHADER_COMMIT "41211fab77a19f4387d55ffffcb713ca1db7cf32")
set(OPENSU_GAMESCOPE_REPOSITORY "https://github.com/SomeoneIsWorking/gamescope.git")
set(OPENSU_GAMESCOPE_COMMIT "93e22887f846ba1445d7d99794f573c90ec04a08")
set(OPENSU_MGBA_LINK_REPOSITORY "https://github.com/SomeoneIsWorking/mgba.git")
set(OPENSU_MGBA_LINK_COMMIT "9011d575f9dad11b948924bdf3e9b27e68257270")
set(OPENSU_SDL_REPOSITORY "https://github.com/SomeoneIsWorking/SDL.git")
set(OPENSU_SDL_COMMIT "3daf0f7f1518493e98aef87f90991214804499c9")
set(OPENSU_SDL_TTF_REPOSITORY "https://github.com/libsdl-org/SDL_ttf.git")
set(OPENSU_SDL_TTF_COMMIT "a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b")
set(OPENSU_SDL_IMAGE_REPOSITORY "https://github.com/libsdl-org/SDL_image.git")
set(OPENSU_SDL_IMAGE_COMMIT "0891fc30428b518e7d018677bd6fa95c1a50f4c6")

# Finds `package` (its variables set in the caller's scope); a prefix built from other pins is
# refused rather than shipped.
macro(opensu_require_thirdparty package commit)
    # Found afresh from CMAKE_PREFIX_PATH on every configure, not from the prefix it named before.
    unset(${package}_DIR CACHE)
    find_package(${package} CONFIG)
    if(NOT ${package}_FOUND)
        message(FATAL_ERROR "${package} is not in CMAKE_PREFIX_PATH (${CMAKE_PREFIX_PATH}). Build "
            "the shared third-party prefix with `uv run --frozen python -m tools.check deps` and "
            "configure with the CMAKE_PREFIX_PATH it prints, or configure with ${ARGN}=OFF.")
    endif()
    if(NOT "${${package}_COMMIT}" STREQUAL "${commit}")
        message(FATAL_ERROR "${${package}_DIR} holds ${package} ${${package}_COMMIT}, not the "
            "pinned ${commit}; `uv run --frozen python -m tools.check deps` builds the pinned one")
    endif()
endmacro()
