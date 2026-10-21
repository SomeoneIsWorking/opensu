add_library(opensu_library STATIC
    game.cpp
    catalog.cpp
    catalog_loader.cpp
    steam.cpp
    epic.cpp
    install_log.cpp
    gog.cpp
    gog_auth.cpp
    gog_token.cpp
    gogdl_auth.cpp
    gog_builds.cpp
    gog_installs.cpp
    roms.cpp
    rom_systems.cpp
    rom_titles.cpp
    param_sfo.cpp
    dump_info.cpp
    switch_crypto.cpp
    switch_keys.cpp
    switch_package.cpp
    switch_nca.cpp
    switch_info.cpp
    arcade_names.cpp
    romset_dat.cpp
    romset_dats.cpp
    romset_lists.cpp
    romset_match.cpp
    shelf.cpp
    library_query.cpp
    play_history.cpp
    play_time.cpp
    emulator_game_list.cpp
    emulator_choice.cpp
    text_fold.cpp
    sections.cpp
    titles.cpp
    emulators.cpp
    emulator_catalog.cpp
    emulator_release.cpp
    downloaded_emulators.cpp
    libretro_cores.cpp
    core_bios.cpp
    appimage_runtime.cpp
    wine.cpp
)
add_library(opensu::library ALIAS opensu_library)
target_include_directories(opensu_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_library
    PUBLIC opensu::vdf
        opensu::setting_layers
        opensu::config
        opensu::net
    PRIVATE opensu::command opensu::fileio opensu::zip lucent::lucent OpenSSL::Crypto
)
target_compile_features(opensu_library PUBLIC cxx_std_20)