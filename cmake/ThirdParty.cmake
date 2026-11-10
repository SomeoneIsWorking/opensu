# The third-party code openSU builds against or ships through the shared prefix (cmake/thirdparty):
# the pinned commits, which that build reads too, and finding them there.

set(OPENSU_PYROWAVE_REPOSITORY "https://github.com/SomeoneIsWorking/pyrowave.git")
set(OPENSU_PYROWAVE_COMMIT "c2c6a4fbb0b7f02898260c3a347c6994b8d8b0c7")
set(OPENSU_GRANITE_REPOSITORY "https://github.com/SomeoneIsWorking/Granite.git")
set(OPENSU_GRANITE_COMMIT "50e424ce93d80d8bd07b60f88edfdcce73c0a59b")
set(OPENSU_LIBRASHADER_REPOSITORY "https://github.com/SomeoneIsWorking/librashader.git")
set(OPENSU_LIBRASHADER_COMMIT "271ac2ead5a9ace657f6edd7f670382073b8b269")
set(OPENSU_GAMESCOPE_REPOSITORY "https://github.com/SomeoneIsWorking/gamescope.git")
set(OPENSU_GAMESCOPE_COMMIT "ecb71a2ef1bc4e4a9a33d414d28fa13efd21d69a")
set(OPENSU_OPUS_REPOSITORY "https://github.com/xiph/opus.git")
set(OPENSU_OPUS_COMMIT "ddbe48383984d56acd9e1ab6a090c54ca6b735a6")
set(OPENSU_SDL_REPOSITORY "https://github.com/SomeoneIsWorking/SDL.git")
set(OPENSU_SDL_COMMIT "6bb22ff47998cd40fb122dacadbf4276c75a25b8")
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
