# Host services behind seams: Bluetooth (BlueZ), power (logind) and the backlight. Each is an
# interface plus a real backend that runs the system's own tools through a `Runner`.
add_library(opensu_host STATIC runner.cpp bluetooth.cpp desktop_request.cpp login_session_end.cpp power.cpp
            session_mode.cpp backlight.cpp secret_store.cpp)
add_library(opensu::host ALIAS opensu_host)
target_include_directories(opensu_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_host PUBLIC opensu::command PRIVATE opensu::fileio lucent::lucent)
target_compile_features(opensu_host PUBLIC cxx_std_20)
target_compile_options(opensu_host PRIVATE -Wall -Wextra -Wpedantic)

# Secrets go to the desktop's keyring through libsecret when the build has it, else to an
# owner-only file. AUTO uses it when found; ON refuses to build without it.
set(OPENSU_SECRET_SERVICE AUTO CACHE STRING "Keep secrets in the Secret Service (AUTO, ON, OFF)")
set_property(CACHE OPENSU_SECRET_SERVICE PROPERTY STRINGS AUTO ON OFF)
if(NOT OPENSU_SECRET_SERVICE STREQUAL "OFF")
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
endif()
if(OPENSU_SECRET_SERVICE STREQUAL "ON" AND NOT LIBSECRET_FOUND)
    message(FATAL_ERROR
        "OPENSU_SECRET_SERVICE=ON needs libsecret. Install it with "
        "`sudo dnf install libsecret-devel` (Fedora), `sudo apt install libsecret-1-dev` "
        "(Debian/Ubuntu) or `sudo pacman -S libsecret` (Arch), or configure with "
        "-DOPENSU_SECRET_SERVICE=AUTO to keep secrets in an owner-only file")
endif()
if(LIBSECRET_FOUND)
    target_sources(opensu_host PRIVATE secret_service_libsecret.cpp)
    target_link_libraries(opensu_host PRIVATE PkgConfig::LIBSECRET)
else()
    target_sources(opensu_host PRIVATE secret_service_none.cpp)
endif()
