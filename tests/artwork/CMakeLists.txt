add_executable(libretro_index_test libretro_index_test.cpp)
target_link_libraries(libretro_index_test PRIVATE iideck::artwork)
target_compile_options(libretro_index_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME libretro_index COMMAND libretro_index_test)

add_executable(artwork_fetcher_test artwork_fetcher_test.cpp)
target_link_libraries(artwork_fetcher_test PRIVATE iideck::artwork lucent::lucent)
target_compile_options(artwork_fetcher_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME artwork_fetcher COMMAND artwork_fetcher_test)
