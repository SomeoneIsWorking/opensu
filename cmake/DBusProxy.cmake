# The xdg-dbus-proxy fork Isolated Steam's filtered desktop bus runs (`steam::SecretBus`). Stock
# xdg-dbus-proxy lets a filtered client signal only names its policy names, so Steam Runtime's
# launcher service cannot answer Steam's launch client; the fork's --peers-talk lets the clients
# of one proxy talk to each other. Built in the shared third-party prefix (cmake/thirdparty) and
# found as `find_package(OpensuDBusProxy)`; a tree only copies the binary, staged and installed at
# <libexecdir>/opensu/xdg-dbus-proxy as Gamescope is.

include(GNUInstallDirs)
include(ThirdParty)

option(OPENSU_BUILD_DBUS_PROXY
    "Stage the pinned xdg-dbus-proxy fork. OFF only for development of the rest of opensu: \
Isolated Steam then refuses to start" ON)

set(OPENSU_DBUS_PROXY_RELATIVE "${CMAKE_INSTALL_LIBEXECDIR}/opensu/xdg-dbus-proxy")

function(opensu_add_dbus_proxy)
    opensu_require_thirdparty(OpensuDBusProxy "${OPENSU_DBUS_PROXY_COMMIT}" OPENSU_BUILD_DBUS_PROXY)
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_DBUS_PROXY_RELATIVE}")
    add_custom_command(OUTPUT "${staged}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OpensuDBusProxy_EXECUTABLE}" "${staged}"
        DEPENDS "${OpensuDBusProxy_EXECUTABLE}"
        COMMENT "Staging xdg-dbus-proxy"
        VERBATIM)
    add_custom_target(opensu_dbus_proxy DEPENDS "${staged}")
    install(PROGRAMS "${staged}" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/opensu")
endfunction()
