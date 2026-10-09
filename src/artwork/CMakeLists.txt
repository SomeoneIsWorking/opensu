add_library(opensu_artwork STATIC
    libretro_index.cpp zip_archive.cpp apk_archive.cpp console_glyphs.cpp iisu_assets.cpp starter_pack.cpp artwork_store.cpp
    artwork_fetcher.cpp)
add_library(opensu::artwork ALIAS opensu_artwork)
target_include_directories(opensu_artwork PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_artwork
    PUBLIC opensu::library opensu::net opensu::audio_model
    PRIVATE ZLIB::ZLIB PkgConfig::WEBP opensu::fileio opensu::raylib lucent::lucent
            Threads::Threads)
target_compile_features(opensu_artwork PUBLIC cxx_std_20)
target_compile_options(opensu_artwork PRIVATE -Wall -Wextra -Wpedantic)
