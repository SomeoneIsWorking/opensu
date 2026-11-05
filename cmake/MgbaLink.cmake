# The mGBA link core (the SomeoneIsWorking/mgba fork, libretro/mgba PR #318) the runner plays
# linked GBA games with: built from the commit cmake/ThirdParty.cmake pins into the shared
# third-party prefix (cmake/thirdparty, `tools.check deps`), found as `find_package(OpensuMgbaLink)`
# and staged where `Config::shippedCores` looks, `<libdir>/opensu/cores/mgba_link_libretro.so`, in
# the build tree as installed. Linux only for now.
#
# OPENSU_BUILD_MGBA_LINK=OFF builds without it: linked play is then not offered.

include(GNUInstallDirs)
include(ThirdParty)

set(OPENSU_SHIPPED_CORES_DIR "${CMAKE_INSTALL_LIBDIR}/opensu/cores")
set(opensu_mgba_link_default OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(opensu_mgba_link_default ON)
endif()
option(OPENSU_BUILD_MGBA_LINK
    "Ship the pinned mGBA fork's core for linked GBA games" ${opensu_mgba_link_default})

function(opensu_add_mgba_link)
    opensu_require_thirdparty(OpensuMgbaLink "${OPENSU_MGBA_LINK_COMMIT}" OPENSU_BUILD_MGBA_LINK)
    get_filename_component(name "${OpensuMgbaLink_LIBRARY}" NAME)
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_SHIPPED_CORES_DIR}/${name}")
    add_custom_command(OUTPUT "${staged}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OpensuMgbaLink_LIBRARY}" "${staged}"
        DEPENDS "${OpensuMgbaLink_LIBRARY}"
        COMMENT "Staging the mGBA link core"
        VERBATIM)
    add_custom_target(opensu_mgba_link ALL DEPENDS "${staged}")
    install(FILES "${staged}" DESTINATION "${OPENSU_SHIPPED_CORES_DIR}")
endfunction()

if(OPENSU_BUILD_MGBA_LINK)
    opensu_add_mgba_link()
endif()
