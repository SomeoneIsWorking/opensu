add_executable(libretro_index_test libretro_index_test.cpp)
target_link_libraries(libretro_index_test PRIVATE opensu::artwork)
target_compile_options(libretro_index_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME libretro_index COMMAND libretro_index_test)

add_executable(artwork_fetcher_test artwork_fetcher_test.cpp)
target_link_libraries(artwork_fetcher_test PRIVATE opensu::artwork lucent::lucent)
target_compile_options(artwork_fetcher_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME artwork_fetcher COMMAND artwork_fetcher_test)

add_executable(apk_archive_test apk_archive_test.cpp)
target_link_libraries(apk_archive_test PRIVATE opensu::artwork ZLIB::ZLIB lucent::lucent
                                               Threads::Threads)
target_include_directories(apk_archive_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(apk_archive_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME apk_archive COMMAND apk_archive_test)

add_executable(iisu_pack_test iisu_pack_test.cpp)
target_link_libraries(iisu_pack_test PRIVATE opensu::artwork ZLIB::ZLIB lucent::lucent)
target_include_directories(iisu_pack_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_definitions(iisu_pack_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
target_compile_options(iisu_pack_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME iisu_pack COMMAND iisu_pack_test)

add_executable(steamgriddb_test steamgriddb_test.cpp)
target_link_libraries(steamgriddb_test PRIVATE opensu::artwork lucent::lucent Threads::Threads)
target_include_directories(steamgriddb_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(steamgriddb_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME steamgriddb COMMAND steamgriddb_test)

add_executable(steamgriddb_covers_test steamgriddb_covers_test.cpp)
target_link_libraries(steamgriddb_covers_test PRIVATE opensu::artwork gpu2d::image lucent::lucent
                                                      Threads::Threads)
target_include_directories(steamgriddb_covers_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_definitions(steamgriddb_covers_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
target_compile_options(steamgriddb_covers_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME steamgriddb_covers COMMAND steamgriddb_covers_test)

# Not a test: the API a hidden openSU is pointed at to show SteamGridDB's covers in a frame.
add_executable(steamgriddb_api_stand_in "${CMAKE_SOURCE_DIR}/tests/support/steamgriddb_api_stand_in.cpp")
target_link_libraries(steamgriddb_api_stand_in PRIVATE gpu2d::image lucent::lucent Threads::Threads)
target_include_directories(steamgriddb_api_stand_in PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(steamgriddb_api_stand_in PRIVATE -Wall -Wextra -Wpedantic)

add_executable(console_theme_test console_theme_test.cpp)
target_link_libraries(console_theme_test PRIVATE opensu::artwork)
target_compile_definitions(console_theme_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
target_compile_options(console_theme_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME console_theme COMMAND console_theme_test)
