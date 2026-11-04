# librashader, which runs libretro slang shader presets in opensu-retro: the pinned fork's C API
# from the shared third-party prefix (cmake/thirdparty, built by `tools.check deps`), found as
# `find_package(OpensuLibrashader)` and staged where the runner loads it at run time,
# `<prefix>/<libdir>/opensu/librashader.so` (`.dll` on Windows, `Contents/Frameworks/librashader.dylib`
# in the macOS app, `Config::librashader`), in the build tree as installed; an Android build packages
# it beside the runner as librashader.so. Its C header librashader.h is MIT and comes from the same
# commit, checked by SHA-256.
#
# OPENSU_BUILD_LIBRASHADER=OFF builds without it: the runner then refuses every shader preset with
# the reason.

include(FetchContent)
include(GNUInstallDirs)
include(ThirdParty)

option(OPENSU_BUILD_LIBRASHADER
    "Use the pinned librashader fork that runs shader presets. OFF builds opensu-retro without \
it: a shader preset is then refused" ON)

set(OPENSU_LIBRASHADER_HEADER_SHA256
    "5d478897c391af3f60015810b67785ae1a286d262a845485276e36ded9f21e62")

# Where the runner finds the library, relative to the directory above its own: openSU's own
# folder in the library directory, or the app bundle's Frameworks on macOS.
if(APPLE)
    set(OPENSU_LIBRASHADER_DIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(OPENSU_LIBRASHADER_DIR "${CMAKE_INSTALL_LIBDIR}/opensu")
endif()
set(OPENSU_LIBRASHADER_RELATIVE
    "${OPENSU_LIBRASHADER_DIR}/librashader${CMAKE_SHARED_LIBRARY_SUFFIX}")

FetchContent_Declare(librashader_header
    URL "https://raw.githubusercontent.com/SomeoneIsWorking/librashader/${OPENSU_LIBRASHADER_COMMIT}/include/librashader.h"
    URL_HASH "SHA256=${OPENSU_LIBRASHADER_HEADER_SHA256}"
    DOWNLOAD_NO_EXTRACT TRUE
    SOURCE_SUBDIR "no-cmake"
)
FetchContent_MakeAvailable(librashader_header)
add_library(opensu_librashader_header INTERFACE)
add_library(opensu::librashader_header ALIAS opensu_librashader_header)
target_include_directories(opensu_librashader_header SYSTEM INTERFACE
    "${librashader_header_SOURCE_DIR}")

function(opensu_add_librashader)
    opensu_require_thirdparty(OpensuLibrashader "${OPENSU_LIBRASHADER_COMMIT}"
        OPENSU_BUILD_LIBRASHADER)
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_LIBRASHADER_RELATIVE}")
    add_custom_command(OUTPUT "${staged}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OpensuLibrashader_LIBRARY}" "${staged}"
        DEPENDS "${OpensuLibrashader_LIBRARY}"
        COMMENT "Staging librashader"
        VERBATIM)
    add_custom_target(opensu_librashader ALL DEPENDS "${staged}")
    install(FILES "${staged}" DESTINATION "${OPENSU_LIBRASHADER_DIR}")
endfunction()

if(OPENSU_BUILD_LIBRASHADER)
    opensu_add_librashader()
endif()
