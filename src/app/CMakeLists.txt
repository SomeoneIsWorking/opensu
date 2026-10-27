add_library(opensu_app STATIC shell_app.cpp wine_runners.cpp plugin_service.cpp plugin_settings.cpp emulator_downloads.cpp emulator_settings.cpp steam_library_settings.cpp artwork_delivery.cpp path_editor.cpp settings_controller.cpp settings_screen.cpp layout_picker.cpp preferences.cpp panel_flow.cpp search_controller.cpp context_menu_controller.cpp frame_png.cpp shell_window.cpp sdl_keys.cpp details_controller.cpp breadcrumb_trail.cpp pointer_router.cpp control_channel.cpp install_job.cpp steam_install_job.cpp cli_install_job.cpp epic_install_job.cpp gog_install_job.cpp installs.cpp uninstall_job.cpp uninstall_prompt.cpp steam_update_job.cpp update_request.cpp launcher_status.cpp sign_in.cpp volume_control.cpp shortcut_editor.cpp shortcut_router.cpp chord_settings.cpp wine_settings.cpp chord_capture.cpp mapping_editor.cpp pad_owner.cpp focus_follow.cpp control_bridge.cpp host_services.cpp brightness_control.cpp audio_outputs.cpp bluetooth_session.cpp controller_roster.cpp devices_controller.cpp devices_screen.cpp retro_session.cpp retro_launch.cpp game_settings.cpp layered_settings.cpp layered_editor.cpp core_option_store.cpp frame_generation.cpp layer_settings.cpp shader_preset_tree.cpp game_end.cpp guide_menu_controller.cpp guide_menu_screen.cpp game_screen.cpp top_bar_clock.cpp keyboard_pad.cpp session_install_controller.cpp launch_mailbox.cpp downloads_board.cpp downloads_controller.cpp downloads_screen.cpp game_launch.cpp integrations_in_force.cpp integration_options.cpp integration_settings.cpp integration_switch.cpp first_run_chooser.cpp steamgriddb_account.cpp artwork_settings.cpp cover_picker_controller.cpp steamgriddb_covers.cpp giveaways_controller.cpp giveaways_screen.cpp offer_claim.cpp)
add_library(opensu::app ALIAS opensu_app)
# What openSU's window is over a running game, and where frame generation is switched: Gamescope
# on Linux, none elsewhere.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(opensu_app PRIVATE game_overlay_gamescope.cpp frame_generation_gamescope.cpp)
else()
    target_sources(opensu_app PRIVATE game_overlay_none.cpp frame_generation_none.cpp)
endif()
target_include_directories(opensu_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# The version About shows: a release passes OPENSU_VERSION, else git describes the checkout when
# the build is configured.
if(NOT DEFINED OPENSU_VERSION)
    execute_process(
        COMMAND git -C "${CMAKE_SOURCE_DIR}" describe --tags --always --dirty
        OUTPUT_VARIABLE OPENSU_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(REGEX REPLACE "^v" "" OPENSU_VERSION "${OPENSU_VERSION}")
endif()
configure_file(build_version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/build_version.hpp" @ONLY)
target_include_directories(opensu_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(opensu_app
    PUBLIC
        opensu::library
        opensu::giveaways
        opensu::plugin
        opensu::artwork
        opensu::audio
        opensu::settings
        opensu::ui
        opensu::device
        opensu::pad
        opensu::launch
        opensu::host
        opensu::steam
        opensu::session
        gpu2d::render
        gpu2d::ui
        lucent::lucent
)
target_compile_features(opensu_app PUBLIC cxx_std_20)