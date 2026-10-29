add_executable(steam_test steam_test.cpp)
target_link_libraries(steam_test PRIVATE opensu::library)
target_compile_definitions(steam_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME steam COMMAND steam_test)

add_executable(roms_test roms_test.cpp)
target_link_libraries(roms_test PRIVATE opensu::library)
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
