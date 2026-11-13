# framegen, the frame pacer and Lossless generator remote play's guest generates frames with, from
# the shared third-party prefix (cmake/thirdparty/FrameGen.cmake, built by `tools.check deps`):
# `framegen::framegen`. Linux and Android (docs/plans/remote-frame-generation.md).

include(ThirdParty)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
    opensu_require_thirdparty(OpensuFrameGen "${OPENSU_GAMESCOPE_COMMIT}")
endif()
