# The opensu shell.
#
# Layout follows one owner per subsystem: the library model and its per-store
# sources, the controller reader, the launch handoff, the Steam client, the nested
# session, host device state, and the drawn shell. Entry points compose; nothing here reaches into
# another's internals.

if(ANDROID)
    add_subdirectory(android)
endif()
add_subdirectory(vdf)
add_subdirectory(clock)
add_subdirectory(config)
add_subdirectory(fileio)
add_subdirectory(secrets)
add_subdirectory(zip)
add_subdirectory(settings)
add_subdirectory(net)
add_subdirectory(audio)
add_subdirectory(library)
add_subdirectory(artwork)
add_subdirectory(giveaways)
add_subdirectory(plugin)
add_subdirectory(gamepad)
add_subdirectory(input)
add_subdirectory(launch)
add_subdirectory(steam)
add_subdirectory(display)
# BlueZ, logind, the backlight, Gamescope, X11 and the login session are Linux's alone.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(host)
    add_subdirectory(session)
endif()
add_subdirectory(lan)
add_subdirectory(stream)
add_subdirectory(device)
add_subdirectory(svg)
add_subdirectory(ui)
add_subdirectory(app)
add_subdirectory(retro)

# On Android the shell is libmain.so, which SDL's Activity loads and enters at SDL_main.
if(ANDROID)
    add_library(opensu SHARED main.cpp)
    set_target_properties(opensu PROPERTIES OUTPUT_NAME main)
else()
    add_executable(opensu main.cpp)
endif()
target_link_libraries(opensu
    PRIVATE
        opensu::app
        opensu::ui
        opensu::pad
        opensu::launch
        opensu::steam
        opensu::library
        lucent::lucent
)
opensu_windowed(opensu)
# PyroWave is linked from openSU's own library directory, where it is installed.
if(TARGET opensu::pyrowave)
    set_property(TARGET opensu APPEND PROPERTY INSTALL_RPATH "\$ORIGIN/../${OPENSU_PYROWAVE_DIR}")
endif()
# Assets sit at share/opensu (Resources/opensu on macOS) beside the executable's directory, in the
# build tree as installed.
file(GLOB_RECURSE opensu_asset_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/*")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets.stamp"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:opensu>/../${CMAKE_INSTALL_DATADIR}/opensu"
    COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/assets.stamp"
    DEPENDS ${opensu_asset_files})
add_custom_target(opensu_assets ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/assets.stamp")
add_dependencies(opensu opensu_assets)

install(TARGETS opensu RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/" DESTINATION "${CMAKE_INSTALL_DATADIR}/opensu")
# Visual Studio's C++ runtime (msvcp140, vcruntime140) beside both executables: Windows ships only
# the UCRT, and a player need not have installed the redistributable. Found in the Visual Studio
# install, so a cross build from Linux (xwin) installs none.
if(MSVC AND CMAKE_HOST_WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    include(InstallRequiredSystemLibraries)
endif()
# The desktop entry and the icon are Linux's; session mode is staged by openSU at run time.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OPENSU_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/opensu")
    # The AppImage passes OPENSU_DESKTOP_EXEC=opensu: whatever integrates it rewrites Exec.
    if(NOT DEFINED OPENSU_DESKTOP_EXEC)
        set(OPENSU_DESKTOP_EXEC "${OPENSU_EXEC}")
    endif()
    configure_file("${CMAKE_SOURCE_DIR}/packaging/opensu.desktop.in"
                   "${CMAKE_BINARY_DIR}/opensu.desktop" @ONLY)
    install(FILES "${CMAKE_BINARY_DIR}/opensu.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/opensu.svg"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
endif()

# Subsystem headers are included as "library/game.hpp" and "vdf/node.hpp", so
# the source root is on the include path for every target.
target_include_directories(opensu_vdf INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_net INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_lan INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_zip INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_audio INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_audio_model INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_library INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_giveaways INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_plugin INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_ui INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_pad INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_input INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_command INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_launch INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_steam INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_app INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu INTERFACE "${CMAKE_SOURCE_DIR}/src")

