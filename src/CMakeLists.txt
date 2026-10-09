# The opensu shell.
#
# Layout follows one owner per subsystem: the library model and its per-store
# sources, the controller reader, the launch handoff, the Steam client, the nested
# session, host device state, and the drawn shell. Entry points compose; nothing here reaches into
# another's internals.

add_subdirectory(vdf)
add_subdirectory(config)
add_subdirectory(fileio)
add_subdirectory(settings)
add_subdirectory(net)
add_subdirectory(audio)
add_subdirectory(library)
add_subdirectory(artwork)
add_subdirectory(gamepad)
add_subdirectory(input)
add_subdirectory(launch)
add_subdirectory(host)
add_subdirectory(steam)
add_subdirectory(session)
add_subdirectory(device)
add_subdirectory(ui)
add_subdirectory(app)

add_executable(opensu main.cpp)
target_link_libraries(opensu
    PRIVATE
        opensu::app
        opensu::ui
        opensu::pad
        opensu::launch
        opensu::steam
        opensu::session
        opensu::library
        opensu::raylib
        lucent::lucent
)
# Assets sit at share/opensu beside the executable's directory, in the build tree as installed.
add_custom_command(TARGET opensu POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:opensu>/../share/opensu")

include(GNUInstallDirs)
install(TARGETS opensu RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/" DESTINATION "${CMAKE_INSTALL_DATADIR}/opensu")
set(OPENSU_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/opensu")
configure_file("${CMAKE_SOURCE_DIR}/packaging/opensu.desktop.in"
               "${CMAKE_BINARY_DIR}/opensu.desktop" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/opensu.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

# Subsystem headers are included as "library/game.hpp" and "vdf/node.hpp", so
# the source root is on the include path for every target.
target_include_directories(opensu_vdf INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_net INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_audio INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_audio_model INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_library INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_ui INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_pad INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_input INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_command INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_launch INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_host INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_steam INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_session INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu_app INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(opensu INTERFACE "${CMAKE_SOURCE_DIR}/src")

