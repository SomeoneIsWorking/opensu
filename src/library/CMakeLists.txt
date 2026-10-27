add_library(opensu_library STATIC
    game.cpp
    catalog.cpp
    catalog_loader.cpp
    steam.cpp
    lossless_scaling.cpp
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
    shader_pack.cpp
    downloaded_emulators.cpp
    libretro_cores.cpp
    core_bios.cpp
    appimage_runtime.cpp
    wine.cpp
    ge_proton.cpp lutris_wine.cpp
    steam_appinfo.cpp
    steam_compat.cpp
    umu_launcher.cpp
    runtime_archive.cpp
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
# The libretro buildbot's nightly folder and core suffix for the OS and CPU built for.
set(opensu_buildbot "https://buildbot.libretro.com/nightly")
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" opensu_cpu)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND opensu_cpu MATCHES "^(x86_64|amd64)$")
    set(opensu_core_folder "${opensu_buildbot}/linux/x86_64/latest/")
    set(opensu_core_suffix ".so")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND opensu_cpu MATCHES "^(x86_64|amd64)$")
    set(opensu_core_folder "${opensu_buildbot}/windows/x86_64/latest/")
    set(opensu_core_suffix ".dll")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND opensu_cpu MATCHES "^(arm64|aarch64)$")
    set(opensu_core_folder "${opensu_buildbot}/apple/osx/arm64/latest/")
    set(opensu_core_suffix ".dylib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android" AND CMAKE_ANDROID_ARCH_ABI MATCHES "^(arm64-v8a|x86_64)$")
    set(opensu_core_folder "${opensu_buildbot}/android/latest/${CMAKE_ANDROID_ARCH_ABI}/")
    set(opensu_core_suffix ".so")
else()
    message(FATAL_ERROR "The libretro buildbot folder for ${CMAKE_SYSTEM_NAME} on "
        "${CMAKE_SYSTEM_PROCESSOR} is not known; add it to src/library/CMakeLists.txt")
endif()
target_compile_definitions(opensu_library PUBLIC
    OPENSU_CORE_BUILDBOT_FOLDER="${opensu_core_folder}"
    OPENSU_CORE_LIBRARY_SUFFIX="${opensu_core_suffix}")
