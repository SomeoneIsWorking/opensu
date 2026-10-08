# The iideck shell.
#
# Layout follows one owner per subsystem: the library model and its per-store
# sources, the controller reader, the launch handoff, the Steam client, the nested
# session, host device state, and the drawn shell. Entry points compose; nothing here reaches into
# another's internals.

add_subdirectory(vdf)
add_subdirectory(config)
add_subdirectory(library)
add_subdirectory(artwork)
add_subdirectory(gamepad)
add_subdirectory(launch)
add_subdirectory(steam)
add_subdirectory(session)
add_subdirectory(device)
add_subdirectory(ui)
add_subdirectory(app)

add_executable(iideck main.cpp)
target_link_libraries(iideck
    PRIVATE
        iideck::app
        iideck::ui
        iideck::pad
        iideck::launch
        iideck::steam
        iideck::session
        iideck::library
        iideck::raylib
        lucent::lucent
)
# Assets sit at share/iideck beside the executable's directory, in the build tree as installed.
add_custom_command(TARGET iideck POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:iideck>/../share/iideck")

include(GNUInstallDirs)
install(TARGETS iideck RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/" DESTINATION "${CMAKE_INSTALL_DATADIR}/iideck")
set(IIDECK_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/iideck")
configure_file("${CMAKE_SOURCE_DIR}/packaging/iideck.desktop.in"
               "${CMAKE_BINARY_DIR}/iideck.desktop" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/iideck.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

# Subsystem headers are included as "library/game.hpp" and "vdf/node.hpp", so
# the source root is on the include path for every target.
target_include_directories(iideck_vdf INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_library INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_ui INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_pad INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_launch INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_steam INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_session INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck_app INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_include_directories(iideck INTERFACE "${CMAKE_SOURCE_DIR}/src")

