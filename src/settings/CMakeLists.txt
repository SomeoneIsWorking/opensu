# The setting layers, the schema declaring what they hold, and the video, frame generation and
# scaling settings: no shell dependencies, so opensu-retro reads the per-launch file with the same owner
# the shell writes it with.
add_library(opensu_setting_layers STATIC setting_layers.cpp setting_choices.cpp setting_schema.cpp
    setting_scope.cpp video_settings.cpp framegen_settings.cpp scaling_settings.cpp
    core_option_settings.cpp)
add_library(opensu::setting_layers ALIAS opensu_setting_layers)
target_link_libraries(opensu_setting_layers PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_setting_layers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_setting_layers PRIVATE lucent::lucent)
target_compile_features(opensu_setting_layers PUBLIC cxx_std_20)
opensu_warnings(opensu_setting_layers)

add_library(opensu_settings STATIC settings.cpp install_folders.cpp integrations.cpp mapping_store.cpp
    game_settings_store.cpp emulator_pick_migration.cpp night_light_choice.cpp asset_owner.cpp)
add_library(opensu::settings ALIAS opensu_settings)
target_link_libraries(opensu_settings PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_settings PUBLIC opensu::library opensu::input opensu::setting_layers
    PRIVATE opensu::fileio lucent::lucent)
target_compile_features(opensu_settings PUBLIC cxx_std_20)
opensu_warnings(opensu_settings)
