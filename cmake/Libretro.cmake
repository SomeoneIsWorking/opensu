# libretro.h and libretro_vulkan.h (MIT), the API a libretro core implements and its Vulkan
# hardware-render interface, from libretro-common at a pinned commit and checked by SHA-256.
# FETCHCONTENT_SOURCE_DIR_LIBRETRO_HEADER and FETCHCONTENT_SOURCE_DIR_LIBRETRO_VULKAN_HEADER point
# at directories holding the files for an offline build.

include(FetchContent)

set(OPENSU_LIBRETRO_COMMIT "a3194c18ba65ef89734e1898a2f61403c255d368")
set(OPENSU_LIBRETRO_SHA256 "036ddffc995ad116c3081ca0a7515d09c2a116f3483e4f5012fe0c29a885da76")
set(OPENSU_LIBRETRO_VULKAN_SHA256 "8c17fe6647069e22779cc5126cef43f8f5f0c83debf2ca2726385b5c8f493f9d")

FetchContent_Declare(libretro_header
    URL "https://raw.githubusercontent.com/libretro/libretro-common/${OPENSU_LIBRETRO_COMMIT}/include/libretro.h"
    URL_HASH "SHA256=${OPENSU_LIBRETRO_SHA256}"
    DOWNLOAD_NO_EXTRACT TRUE
    SOURCE_SUBDIR "no-cmake"
)
FetchContent_Declare(libretro_vulkan_header
    URL "https://raw.githubusercontent.com/libretro/libretro-common/${OPENSU_LIBRETRO_COMMIT}/include/libretro_vulkan.h"
    URL_HASH "SHA256=${OPENSU_LIBRETRO_VULKAN_SHA256}"
    DOWNLOAD_NO_EXTRACT TRUE
    SOURCE_SUBDIR "no-cmake"
)
FetchContent_MakeAvailable(libretro_header libretro_vulkan_header)

add_library(opensu_libretro_header INTERFACE)
add_library(opensu::libretro_header ALIAS opensu_libretro_header)
target_include_directories(opensu_libretro_header SYSTEM
    INTERFACE "${libretro_header_SOURCE_DIR}" "${libretro_vulkan_header_SOURCE_DIR}")
