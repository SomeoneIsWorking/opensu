# libretro.h (MIT), the API a libretro core implements, from libretro-common at a pinned commit and
# checked by SHA-256. FETCHCONTENT_SOURCE_DIR_LIBRETRO_HEADER points at a directory holding the file
# for an offline build.

include(FetchContent)

set(OPENSU_LIBRETRO_COMMIT "a3194c18ba65ef89734e1898a2f61403c255d368")
set(OPENSU_LIBRETRO_SHA256 "036ddffc995ad116c3081ca0a7515d09c2a116f3483e4f5012fe0c29a885da76")

FetchContent_Declare(libretro_header
    URL "https://raw.githubusercontent.com/libretro/libretro-common/${OPENSU_LIBRETRO_COMMIT}/include/libretro.h"
    URL_HASH "SHA256=${OPENSU_LIBRETRO_SHA256}"
    DOWNLOAD_NO_EXTRACT TRUE
    SOURCE_SUBDIR "no-cmake"
)
FetchContent_MakeAvailable(libretro_header)

add_library(opensu_libretro_header INTERFACE)
add_library(opensu::libretro_header ALIAS opensu_libretro_header)
target_include_directories(opensu_libretro_header SYSTEM INTERFACE "${libretro_header_SOURCE_DIR}")
