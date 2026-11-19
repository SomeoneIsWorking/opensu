# The xdg-dbus-proxy fork (LGPL-2.1-or-later) from its pinned commit, built with meson and ninja in
# the build environment (glib and gio), installed at `libexec/opensu/xdg-dbus-proxy`.

foreach(tool meson ninja)
    find_program(OPENSU_DBUS_PROXY_${tool} ${tool})
    if(NOT OPENSU_DBUS_PROXY_${tool})
        message(FATAL_ERROR "The xdg-dbus-proxy fork needs ${tool} and glib's development files "
            "(Fedora: `sudo dnf builddep xdg-dbus-proxy`); tools.check's build image has them.")
    endif()
endforeach()

set(root "${CMAKE_BINARY_DIR}/dbus-proxy")
set(built "${root}/build/xdg-dbus-proxy")
set(compilers "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}")

ExternalProject_Add(opensu_dbus_proxy
    GIT_REPOSITORY "${OPENSU_DBUS_PROXY_REPOSITORY}"
    GIT_TAG "${OPENSU_DBUS_PROXY_COMMIT}"
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/source"
    BINARY_DIR "${root}/build"
    CONFIGURE_COMMAND ${compilers} "${OPENSU_DBUS_PROXY_meson}" setup "${root}/build"
        "${root}/source" --buildtype=release -Dman=disabled -Dtests=false
    BUILD_COMMAND ${compilers} "${OPENSU_DBUS_PROXY_ninja}" -C "${root}/build" xdg-dbus-proxy
    BUILD_BYPRODUCTS "${built}"
    INSTALL_COMMAND ""
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuDBusProxyConfig.cmake" @ONLY CONTENT [=[
# The xdg-dbus-proxy fork from openSU's shared third-party prefix, at the commit openSU pins.
get_filename_component(_opensu_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpensuDBusProxy_COMMIT "@OPENSU_DBUS_PROXY_COMMIT@")
set(OpensuDBusProxy_EXECUTABLE "${_opensu_prefix}/libexec/opensu/xdg-dbus-proxy")
unset(_opensu_prefix)
]=])
install(PROGRAMS "${built}" DESTINATION libexec/opensu)
install(FILES "${root}/OpensuDBusProxyConfig.cmake" DESTINATION lib/cmake/OpensuDBusProxy)
