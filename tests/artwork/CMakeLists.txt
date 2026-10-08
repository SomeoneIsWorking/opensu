add_executable(libretro_index_test libretro_index_test.cpp)
target_link_libraries(libretro_index_test PRIVATE iideck::artwork)
target_compile_options(libretro_index_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME libretro_index COMMAND libretro_index_test)

add_executable(artwork_fetcher_test artwork_fetcher_test.cpp)
target_link_libraries(artwork_fetcher_test PRIVATE iideck::artwork lucent::lucent)
target_compile_options(artwork_fetcher_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME artwork_fetcher COMMAND artwork_fetcher_test)

add_executable(zip_archive_test zip_archive_test.cpp)
target_link_libraries(zip_archive_test PRIVATE iideck::artwork ZLIB::ZLIB)
target_compile_options(zip_archive_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME zip_archive COMMAND zip_archive_test)

add_executable(starter_pack_test starter_pack_test.cpp)
target_link_libraries(starter_pack_test PRIVATE iideck::artwork ZLIB::ZLIB PkgConfig::WEBP
                                                lucent::lucent Threads::Threads)
target_include_directories(starter_pack_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(starter_pack_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME starter_pack COMMAND starter_pack_test)

add_executable(iisu_assets_test iisu_assets_test.cpp)
target_link_libraries(iisu_assets_test PRIVATE iideck::artwork)
target_include_directories(iisu_assets_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(iisu_assets_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME iisu_assets COMMAND iisu_assets_test)
