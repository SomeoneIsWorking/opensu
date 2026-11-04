# The third-party code openSU builds against or ships through the shared prefix (cmake/thirdparty):
# the pinned commits, which that build reads too, and finding them there.

set(OPENSU_PYROWAVE_REPOSITORY "https://github.com/SomeoneIsWorking/pyrowave.git")
set(OPENSU_PYROWAVE_COMMIT "92014b63d5db0e15dca4fb1e301e0b784cd290da")
set(OPENSU_GRANITE_REPOSITORY "https://github.com/Themaister/Granite.git")
set(OPENSU_GRANITE_COMMIT "53090429539bf3404250d18fe58db971f0907c04")
set(OPENSU_LIBRASHADER_REPOSITORY "https://github.com/SomeoneIsWorking/librashader.git")
set(OPENSU_LIBRASHADER_COMMIT "41211fab77a19f4387d55ffffcb713ca1db7cf32")
set(OPENSU_GAMESCOPE_REPOSITORY "https://github.com/SomeoneIsWorking/gamescope.git")
set(OPENSU_GAMESCOPE_COMMIT "7bc32a74efb9525e07f215e7326d418247c74021")

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
