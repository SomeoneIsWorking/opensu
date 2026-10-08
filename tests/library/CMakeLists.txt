add_executable(steam_test steam_test.cpp)
target_link_libraries(steam_test PRIVATE iideck::library)
add_test(NAME steam COMMAND steam_test)

add_executable(roms_test roms_test.cpp)
target_link_libraries(roms_test PRIVATE iideck::library)
add_test(NAME roms COMMAND roms_test)

add_executable(shelf_test shelf_test.cpp)
target_link_libraries(shelf_test PRIVATE iideck::library)
add_test(NAME shelf COMMAND shelf_test)

add_executable(gog_test gog_test.cpp)
target_link_libraries(gog_test PRIVATE iideck::library)
target_include_directories(gog_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(gog_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME gog COMMAND gog_test)

add_executable(epic_test epic_test.cpp)
target_link_libraries(epic_test PRIVATE iideck::library)
target_compile_definitions(epic_test PRIVATE IIDECK_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME epic COMMAND epic_test)

add_executable(titles_test titles_test.cpp)
target_link_libraries(titles_test PRIVATE iideck::library)
add_test(NAME titles COMMAND titles_test)

add_executable(sections_test sections_test.cpp)
target_link_libraries(sections_test PRIVATE iideck::library)
target_include_directories(sections_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(sections_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sections COMMAND sections_test)
