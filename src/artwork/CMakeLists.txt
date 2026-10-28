add_library(opensu_artwork STATIC
    libretro_index.cpp apk_archive.cpp iisu_assets.cpp artwork_store.cpp
    artwork_fetcher.cpp arcade_dat.cpp steamgriddb.cpp cover_overrides.cpp cover_fitter.cpp
    cover_offers.cpp)
add_library(opensu::artwork ALIAS opensu_artwork)
target_include_directories(opensu_artwork PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_artwork
    PUBLIC opensu::library opensu::net opensu::zip opensu::audio_model
    PRIVATE ZLIB::ZLIB gpu2d::image opensu::fileio lucent::lucent
            Threads::Threads)
target_compile_features(opensu_artwork PUBLIC cxx_std_20)
target_compile_options(opensu_artwork PRIVATE -Wall -Wextra -Wpedantic)
