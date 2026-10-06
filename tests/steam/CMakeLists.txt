add_executable(desktop_steam_test desktop_steam_test.cpp)
target_link_libraries(desktop_steam_test PRIVATE iideck::steam)
add_test(NAME desktop_steam COMMAND desktop_steam_test)

add_executable(steam_client_test client_test.cpp)
target_link_libraries(steam_client_test PRIVATE iideck::steam lucent::lucent Threads::Threads)
add_test(NAME steam_client COMMAND steam_client_test)
set_tests_properties(steam_client PROPERTIES TIMEOUT 180)

foreach(test desktop_steam_test steam_client_test)
    target_compile_definitions(${test} PRIVATE IIDECK_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
endforeach()
