add_executable(steam_test steam_test.cpp)
target_link_libraries(steam_test PRIVATE iideck::library)
add_test(NAME steam COMMAND steam_test)
