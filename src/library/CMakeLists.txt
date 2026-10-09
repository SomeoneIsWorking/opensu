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
    gog_installs.cpp
    roms.cpp
    rom_systems.cpp
    rom_titles.cpp
    arcade_names.cpp
    shelf.cpp
    library_query.cpp
    play_history.cpp
    text_fold.cpp
    sections.cpp
    titles.cpp
    emulators.cpp
)
add_library(opensu::library ALIAS opensu_library)
target_include_directories(opensu_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_library
    PUBLIC opensu::vdf
        opensu::config
        opensu::net
    PRIVATE opensu::command opensu::fileio lucent::lucent
)
target_compile_features(opensu_library PUBLIC cxx_std_20)