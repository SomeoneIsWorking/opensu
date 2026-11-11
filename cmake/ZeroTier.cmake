# The embedded ZeroTier node Play online dials beside the direct path: Lucent's `lucent::zerotier`
# on libzt from the shared third-party prefix (cmake/thirdparty/Libzt.cmake, built by
# `tools.check deps`).

include(ThirdParty)

option(OPENSU_BUILD_ZEROTIER
    "Play online also over an embedded ZeroTier node, which reaches past NATs a direct path cannot"
    ON)

if(OPENSU_BUILD_ZEROTIER)
    opensu_require_thirdparty(OpensuLibzt "${OPENSU_LIBZT_COMMIT}" OPENSU_BUILD_ZEROTIER)
    # Lucent finds libzt itself: the one beside the commit just checked, not a prefix cached before.
    set(libzt_DIR "${OpensuLibzt_DIR}/../libzt" CACHE PATH "libzt from the pinned prefix" FORCE)
endif()
set(LUCENT_BUILD_ZEROTIER ${OPENSU_BUILD_ZEROTIER} CACHE BOOL
    "openSU dials over Lucent's embedded ZeroTier" FORCE)
