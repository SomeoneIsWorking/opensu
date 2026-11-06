# Android's Opus for remote play's sound, from the shared third-party prefix (cmake/thirdparty,
# built by `tools.check deps`); the desktop finds its own (src/stream).

include(ThirdParty)

if(ANDROID)
    opensu_require_thirdparty(OpensuOpus "${OPENSU_OPUS_COMMIT}")
    add_library(opensu_opus_library STATIC IMPORTED GLOBAL)
    add_library(opensu::opus ALIAS opensu_opus_library)
    set_target_properties(opensu_opus_library PROPERTIES
        IMPORTED_LOCATION "${OpensuOpus_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OpensuOpus_INCLUDE_DIR}")
endif()
