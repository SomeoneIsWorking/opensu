# The embedded ZeroTier node Play online dials beside the direct path: Lucent's `lucent::zerotier`
# on libzt from the shared third-party prefix (cmake/thirdparty/Libzt.cmake, built by
# `tools.check deps`). Not on Windows yet, whose libzt build the fork has still to fix.

include(ThirdParty)

if(WIN32)
    set(_opensu_zerotier_default OFF)
else()
    set(_opensu_zerotier_default ON)
endif()
option(OPENSU_BUILD_ZEROTIER
    "Play online also over an embedded ZeroTier node, which reaches past NATs a direct path cannot"
    ${_opensu_zerotier_default})

if(OPENSU_BUILD_ZEROTIER)
    opensu_require_thirdparty(OpensuLibzt "${OPENSU_LIBZT_COMMIT}" OPENSU_BUILD_ZEROTIER)
endif()
set(LUCENT_BUILD_ZEROTIER ${OPENSU_BUILD_ZEROTIER} CACHE BOOL
    "openSU dials over Lucent's embedded ZeroTier" FORCE)
