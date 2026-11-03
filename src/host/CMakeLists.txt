# Host services behind seams: Bluetooth (BlueZ), power (logind) and the backlight. Each is an
# interface plus a real backend that runs the system's own tools through a `Runner`. openSU's
# polkit agent serves an object on the system bus, which needs sd-bus.
add_library(opensu_host STATIC runner.cpp bluetooth.cpp desktop_request.cpp login_session_end.cpp power.cpp
            session_mode.cpp backlight.cpp desktop_night_light.cpp polkit_helper.cpp
            polkit_authentication.cpp polkit_agent.cpp)
add_library(opensu::host ALIAS opensu_host)
target_link_libraries(opensu_host PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" INTERFACE "${CMAKE_SOURCE_DIR}/src")
pkg_check_modules(LIBSYSTEMD IMPORTED_TARGET libsystemd)
if(NOT LIBSYSTEMD_FOUND)
    message(FATAL_ERROR
        "openSU's polkit agent needs sd-bus from libsystemd. Install it with "
        "`sudo dnf install systemd-devel` (Fedora), `sudo apt install libsystemd-dev` "
        "(Debian/Ubuntu) or `sudo pacman -S systemd-libs` (Arch)")
endif()
target_link_libraries(opensu_host PUBLIC opensu::command
                      PRIVATE opensu::fileio opensu::launch opensu::secrets lucent::lucent
                              PkgConfig::LIBSYSTEMD)
target_compile_features(opensu_host PUBLIC cxx_std_20)
opensu_warnings(opensu_host)


# The root-run scripts, built in so session mode is staged from the binary itself.
set(opensu_session_scripts "${CMAKE_SOURCE_DIR}/packaging/install-session.sh"
    "${CMAKE_SOURCE_DIR}/packaging/opensu-session-select.sh")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${opensu_session_scripts})
file(READ "${CMAKE_SOURCE_DIR}/packaging/install-session.sh" OPENSU_INSTALL_SESSION_SH)
file(READ "${CMAKE_SOURCE_DIR}/packaging/opensu-session-select.sh" OPENSU_SESSION_SELECT_SH)
configure_file(session_scripts.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/session_scripts.hpp" @ONLY)
target_include_directories(opensu_host PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
