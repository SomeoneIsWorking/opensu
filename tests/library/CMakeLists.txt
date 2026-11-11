add_executable(steam_test steam_test.cpp)
target_link_libraries(steam_test PRIVATE opensu::library)
target_compile_definitions(steam_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME steam COMMAND steam_test)

add_executable(roms_test roms_test.cpp)
target_link_libraries(roms_test PRIVATE opensu::library lucent::lucent ZLIB::ZLIB)
target_compile_definitions(roms_test PRIVATE OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
add_test(NAME roms COMMAND roms_test)

add_executable(rom_titles_test rom_titles_test.cpp)
target_link_libraries(rom_titles_test PRIVATE opensu::library)
add_test(NAME rom_titles COMMAND rom_titles_test)

add_executable(arcade_names_test arcade_names_test.cpp)
target_link_libraries(arcade_names_test PRIVATE opensu::library)
add_test(NAME arcade_names COMMAND arcade_names_test)

add_executable(shelf_test shelf_test.cpp)
target_link_libraries(shelf_test PRIVATE opensu::library)
add_test(NAME shelf COMMAND shelf_test)

add_executable(linked_play_test linked_play_test.cpp)
target_link_libraries(linked_play_test PRIVATE opensu::library)
target_include_directories(linked_play_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/../support")
add_test(NAME linked_play COMMAND linked_play_test)

add_executable(gog_test gog_test.cpp)
target_link_libraries(gog_test PRIVATE opensu::library)
opensu_warnings(gog_test)
add_test(NAME gog COMMAND gog_test)

add_executable(epic_test epic_test.cpp)
target_link_libraries(epic_test PRIVATE opensu::library)
target_compile_definitions(epic_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch"
    OPENSU_LEGENDARY_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/legendary")
add_test(NAME epic COMMAND epic_test)

add_executable(wine_test wine_test.cpp)
target_link_libraries(wine_test PRIVATE opensu::library)
target_compile_definitions(wine_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME wine COMMAND wine_test)

add_executable(titles_test titles_test.cpp)
target_link_libraries(titles_test PRIVATE opensu::library)
add_test(NAME titles COMMAND titles_test)

add_executable(sections_test sections_test.cpp)
target_link_libraries(sections_test PRIVATE opensu::library)
opensu_warnings(sections_test)
add_test(NAME sections COMMAND sections_test)

add_executable(gog_install_test gog_install_test.cpp)
target_link_libraries(gog_install_test PRIVATE opensu::library)
opensu_warnings(gog_install_test)
add_test(NAME gog_install COMMAND gog_install_test)

add_executable(install_log_test install_log_test.cpp)
target_link_libraries(install_log_test PRIVATE opensu::library)
opensu_warnings(install_log_test)
add_test(NAME install_log COMMAND install_log_test)

add_executable(catalog_loader_test catalog_loader_test.cpp)
target_link_libraries(catalog_loader_test PRIVATE opensu::library)
opensu_warnings(catalog_loader_test)
add_test(NAME catalog_loader COMMAND catalog_loader_test)

add_executable(library_query_test library_query_test.cpp)
target_link_libraries(library_query_test PRIVATE opensu::library)
add_test(NAME library_query COMMAND library_query_test)

add_executable(emulator_choice_test emulator_choice_test.cpp)
target_link_libraries(emulator_choice_test PRIVATE opensu::library)
add_test(NAME emulator_choice COMMAND emulator_choice_test)

add_executable(match_pick_test match_pick_test.cpp)
target_link_libraries(match_pick_test PRIVATE opensu::library)
add_test(NAME match_pick COMMAND match_pick_test)

add_executable(emulator_catalog_test emulator_catalog_test.cpp)
target_link_libraries(emulator_catalog_test PRIVATE opensu::library)
target_compile_definitions(emulator_catalog_test PRIVATE OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
add_test(NAME emulator_catalog COMMAND emulator_catalog_test)

add_executable(emulator_release_test emulator_release_test.cpp)
target_link_libraries(emulator_release_test PRIVATE opensu::library lucent::lucent nlohmann_json::nlohmann_json)
target_compile_definitions(emulator_release_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME emulator_release COMMAND emulator_release_test)

add_executable(play_time_test play_time_test.cpp)
target_link_libraries(play_time_test PRIVATE opensu::library)
opensu_warnings(play_time_test)
add_test(NAME play_time COMMAND play_time_test)

add_executable(libretro_cores_test libretro_cores_test.cpp)
target_link_libraries(libretro_cores_test PRIVATE opensu::library lucent::lucent ZLIB::ZLIB)
opensu_warnings(libretro_cores_test)
target_compile_definitions(libretro_cores_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME libretro_cores COMMAND libretro_cores_test)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME libretro_cores_standard_search COMMAND libretro_cores_test linux-standard-search)
endif()

add_executable(core_bios_test core_bios_test.cpp)
target_link_libraries(core_bios_test PRIVATE opensu::library lucent::lucent ZLIB::ZLIB)
opensu_warnings(core_bios_test)
target_compile_definitions(core_bios_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME core_bios COMMAND core_bios_test)

add_executable(romset_convert_test romset_convert_test.cpp)
target_link_libraries(romset_convert_test PRIVATE opensu::library opensu::zip lucent::lucent ZLIB::ZLIB)
opensu_warnings(romset_convert_test)
target_compile_definitions(romset_convert_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME romset_convert COMMAND romset_convert_test)

add_executable(romset_match_test romset_match_test.cpp)
target_link_libraries(romset_match_test PRIVATE opensu::library lucent::lucent ZLIB::ZLIB)
opensu_warnings(romset_match_test)
target_compile_definitions(romset_match_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME romset_match COMMAND romset_match_test)

add_executable(switch_info_test switch_info_test.cpp)
target_link_libraries(switch_info_test PRIVATE opensu::library OpenSSL::Crypto)
target_compile_definitions(switch_info_test PRIVATE
    OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch"
    OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
add_test(NAME switch_info COMMAND switch_info_test)

add_executable(game_saves_test game_saves_test.cpp)
target_link_libraries(game_saves_test PRIVATE opensu::library nlohmann_json::nlohmann_json)
target_compile_definitions(game_saves_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(game_saves_test)
add_test(NAME game_saves COMMAND game_saves_test)

add_executable(steam_compat_test steam_compat_test.cpp)
target_link_libraries(steam_compat_test PRIVATE opensu::library)
target_compile_definitions(steam_compat_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME steam_compat COMMAND steam_compat_test)

add_executable(own_games_test own_games_test.cpp)
target_link_libraries(own_games_test PRIVATE opensu::library PkgConfig::SQLITE3)
target_compile_definitions(own_games_test PRIVATE
    OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME own_games COMMAND own_games_test)

add_executable(game_identify_test game_identify_test.cpp)
target_link_libraries(game_identify_test PRIVATE opensu::library)
target_compile_definitions(game_identify_test PRIVATE
    OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME game_identify COMMAND game_identify_test)

add_executable(game_folder_test game_folder_test.cpp)
target_link_libraries(game_folder_test PRIVATE opensu::library)
target_include_directories(game_folder_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(game_folder_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME game_folder COMMAND game_folder_test)

add_executable(cemu_pads_test cemu_pads_test.cpp)
target_link_libraries(cemu_pads_test PRIVATE opensu::library)
target_compile_definitions(cemu_pads_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(cemu_pads_test)
add_test(NAME cemu_pads COMMAND cemu_pads_test)

add_executable(launch_files_test launch_files_test.cpp)
target_link_libraries(launch_files_test PRIVATE opensu::library)
target_compile_definitions(launch_files_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(launch_files_test)
add_test(NAME launch_files COMMAND launch_files_test)

add_executable(yuzu_config_test yuzu_config_test.cpp)
target_link_libraries(yuzu_config_test PRIVATE opensu::library)
opensu_warnings(yuzu_config_test)
add_test(NAME yuzu_config COMMAND yuzu_config_test)

add_executable(dolphin_config_test dolphin_config_test.cpp)
target_link_libraries(dolphin_config_test PRIVATE opensu::library)
target_compile_definitions(dolphin_config_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch"
    OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
opensu_warnings(dolphin_config_test)
add_test(NAME dolphin_config COMMAND dolphin_config_test)

add_executable(mgba_config_test mgba_config_test.cpp)
target_link_libraries(mgba_config_test PRIVATE opensu::library)
target_compile_definitions(mgba_config_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(mgba_config_test)
add_test(NAME mgba_config COMMAND mgba_config_test)

add_executable(ryujinx_config_test ryujinx_config_test.cpp)
target_link_libraries(ryujinx_config_test PRIVATE opensu::library nlohmann_json::nlohmann_json)
opensu_warnings(ryujinx_config_test)
add_test(NAME ryujinx_config COMMAND ryujinx_config_test)

add_executable(azahar_config_test azahar_config_test.cpp)
target_link_libraries(azahar_config_test PRIVATE opensu::library)
opensu_warnings(azahar_config_test)
add_test(NAME azahar_config COMMAND azahar_config_test)

add_executable(pcsx2_config_test pcsx2_config_test.cpp)
target_link_libraries(pcsx2_config_test PRIVATE opensu::library)
opensu_warnings(pcsx2_config_test)
add_test(NAME pcsx2_config COMMAND pcsx2_config_test)

add_executable(melonds_config_test melonds_config_test.cpp)
target_link_libraries(melonds_config_test PRIVATE opensu::library)
opensu_warnings(melonds_config_test)
add_test(NAME melonds_config COMMAND melonds_config_test)

add_executable(rpcs3_config_test rpcs3_config_test.cpp)
target_link_libraries(rpcs3_config_test PRIVATE opensu::library)
opensu_warnings(rpcs3_config_test)
add_test(NAME rpcs3_config COMMAND rpcs3_config_test)

add_executable(duckstation_config_test duckstation_config_test.cpp)
target_link_libraries(duckstation_config_test PRIVATE opensu::library)
target_compile_definitions(duckstation_config_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(duckstation_config_test)
add_test(NAME duckstation_config COMMAND duckstation_config_test)

add_executable(xemu_config_test xemu_config_test.cpp)
target_link_libraries(xemu_config_test PRIVATE opensu::library)
opensu_warnings(xemu_config_test)
add_test(NAME xemu_config COMMAND xemu_config_test)

add_executable(visual_presets_test visual_presets_test.cpp)
target_link_libraries(visual_presets_test PRIVATE opensu::library)
target_compile_definitions(visual_presets_test PRIVATE OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(visual_presets_test)
add_test(NAME visual_presets COMMAND visual_presets_test)
