add_executable(steam_test steam_test.cpp)
target_link_libraries(steam_test PRIVATE opensu::library)
add_test(NAME steam COMMAND steam_test)

add_executable(roms_test roms_test.cpp)
target_link_libraries(roms_test PRIVATE opensu::library)
add_test(NAME roms COMMAND roms_test)

add_executable(rom_titles_test rom_titles_test.cpp)
target_link_libraries(rom_titles_test PRIVATE opensu::library)
add_test(NAME rom_titles COMMAND rom_titles_test)

add_executable(arcade_names_test arcade_names_test.cpp)
target_link_libraries(arcade_names_test PRIVATE opensu::library)
add_test(NAME arcade_names COMMAND arcade_names_test)

add_executable(shelf_test shelf_test.cpp)
target_link_libraries(shelf_test PRIVATE opensu::library)
add_test(NAME shelf COMMAND shelf_test)

add_executable(gog_test gog_test.cpp)
target_link_libraries(gog_test PRIVATE opensu::library)
target_include_directories(gog_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(gog_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME gog COMMAND gog_test)

add_executable(epic_test epic_test.cpp)
target_link_libraries(epic_test PRIVATE opensu::library)
target_compile_definitions(epic_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME epic COMMAND epic_test)

add_executable(titles_test titles_test.cpp)
target_link_libraries(titles_test PRIVATE opensu::library)
add_test(NAME titles COMMAND titles_test)

add_executable(sections_test sections_test.cpp)
target_link_libraries(sections_test PRIVATE opensu::library)
target_include_directories(sections_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(sections_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sections COMMAND sections_test)

add_executable(gog_install_test gog_install_test.cpp)
target_link_libraries(gog_install_test PRIVATE opensu::library)
target_compile_options(gog_install_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME gog_install COMMAND gog_install_test)

add_executable(install_log_test install_log_test.cpp)
target_link_libraries(install_log_test PRIVATE opensu::library)
target_compile_options(install_log_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME install_log COMMAND install_log_test)

add_executable(catalog_loader_test catalog_loader_test.cpp)
target_link_libraries(catalog_loader_test PRIVATE opensu::library)
target_compile_options(catalog_loader_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME catalog_loader COMMAND catalog_loader_test)

add_executable(library_query_test library_query_test.cpp)
target_link_libraries(library_query_test PRIVATE opensu::library)
target_include_directories(library_query_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
add_test(NAME library_query COMMAND library_query_test)
