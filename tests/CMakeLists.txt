add_executable(vdf_test vdf/vdf_test.cpp)
target_link_libraries(vdf_test PRIVATE opensu::vdf)
add_test(NAME vdf COMMAND vdf_test)

add_subdirectory(config)
add_subdirectory(settings)
add_subdirectory(library)
add_subdirectory(plugin)
add_subdirectory(net)
add_subdirectory(artwork)
add_subdirectory(giveaways)
add_subdirectory(zip)
add_subdirectory(audio)
add_subdirectory(launch)
add_subdirectory(fileio)
add_subdirectory(secrets)
add_subdirectory(steam)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(host)
    add_subdirectory(session)
endif()
add_subdirectory(device)
add_subdirectory(ui)
add_subdirectory(gamepad)
add_subdirectory(input)
add_subdirectory(app)
add_subdirectory(retro)

# Tests whose fakes are POSIX: shell-script stand-ins for programs, signals, fork, raw sockets,
# Unix file modes and execute bits, XDG directories, sysfs or evdev; and tests of Linux-only
# features (Wine and Proton runners). Windows leaves them unbuilt and reports them as not run; its
# own backends are covered by tests/launch's win32 test.
if(WIN32)
    set_target_properties(
        handoff_test process_launcher_test process_tree_test command_test
        roms_test gog_test epic_test gog_install_test config_test install_folders_test
        desktop_steam_test steam_client_test steam_devtools_test steamgriddb_sign_in_test
        install_job_test settings_controller_test retro_session_test game_end_test
        downloads_controller_test update_job_test pad_translator_test pads_test secret_store_test
        retro_save_memory_test retro_runner_test wine_test steam_compat_test own_games_test
        battery_test sign_in_test uninstall_job_test
        PROPERTIES EXCLUDE_FROM_ALL TRUE)
    get_property(opensu_config_tests DIRECTORY config PROPERTY TESTS)
    set_property(TEST handoff process_launcher process_tree command DIRECTORY launch
        PROPERTY DISABLED TRUE)
    set_property(TEST roms gog epic gog_install wine steam_compat own_games DIRECTORY library
        PROPERTY DISABLED TRUE)
    set_property(TEST battery DIRECTORY device PROPERTY DISABLED TRUE)
    set_property(TEST ${opensu_config_tests} DIRECTORY config PROPERTY DISABLED TRUE)
    set_property(TEST install_folders DIRECTORY settings PROPERTY DISABLED TRUE)
    set_property(TEST desktop_steam steam_client steam_devtools steamgriddb_sign_in
        DIRECTORY steam PROPERTY DISABLED TRUE)
    set_property(TEST install_job settings_controller retro_session game_end downloads_controller
        update_job sign_in uninstall_job DIRECTORY app PROPERTY DISABLED TRUE)
    set_property(TEST pad_translator pads DIRECTORY gamepad PROPERTY DISABLED TRUE)
    set_property(TEST secret_store DIRECTORY secrets PROPERTY DISABLED TRUE)
    set_property(TEST retro_save_memory retro_runner DIRECTORY retro PROPERTY DISABLED TRUE)
endif()

# Tests of the Linux backends themselves (systemd scopes, /proc): macOS has its own in
# tests/launch's darwin test.
if(APPLE)
    set_target_properties(handoff_test process_launcher_test process_tree_test desktop_steam_test
        steam_client_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_property(TEST handoff process_launcher process_tree DIRECTORY launch PROPERTY DISABLED TRUE)
    set_property(TEST desktop_steam steam_client DIRECTORY steam PROPERTY DISABLED TRUE)
endif()
