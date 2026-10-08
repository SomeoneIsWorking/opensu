add_executable(steam_test steam_test.cpp)
target_link_libraries(steam_test PRIVATE iideck::library)
add_test(NAME steam COMMAND steam_test)

add_executable(roms_test roms_test.cpp)
target_link_libraries(roms_test PRIVATE iideck::library)
add_test(NAME roms COMMAND roms_test)

add_executable(shelf_test shelf_test.cpp)
target_link_libraries(shelf_test PRIVATE iideck::library)
add_test(NAME shelf COMMAND shelf_test)
