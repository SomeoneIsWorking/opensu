add_library(iideck_artwork STATIC
    libretro_index.cpp zip_archive.cpp apk_archive.cpp console_glyphs.cpp starter_pack.cpp artwork_store.cpp
    artwork_fetcher.cpp)
add_library(iideck::artwork ALIAS iideck_artwork)
target_include_directories(iideck_artwork PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_artwork
    PUBLIC iideck::library iideck::net iideck::audio_model
    PRIVATE ZLIB::ZLIB PkgConfig::WEBP iideck::raylib lucent::lucent
            Threads::Threads)
target_compile_features(iideck_artwork PUBLIC cxx_std_20)
target_compile_options(iideck_artwork PRIVATE -Wall -Wextra -Wpedantic)
