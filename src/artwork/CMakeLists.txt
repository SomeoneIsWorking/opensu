add_library(opensu_artwork STATIC
    libretro_index.cpp apk_archive.cpp iisu_pack.cpp iisu_install.cpp console_theme.cpp console_themes.cpp artwork_store.cpp
    artwork_fetcher.cpp arcade_dat.cpp steamgriddb.cpp cover_overrides.cpp cover_fitter.cpp
    cover_offers.cpp)
add_library(opensu::artwork ALIAS opensu_artwork)
target_link_libraries(opensu_artwork PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_artwork PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_artwork
    PUBLIC opensu::library opensu::net opensu::zip opensu::audio_model
    PRIVATE ZLIB::ZLIB gpu2d::image opensu::fileio opensu::secrets lucent::lucent
            Threads::Threads)
target_compile_features(opensu_artwork PUBLIC cxx_std_20)
opensu_warnings(opensu_artwork)
