add_executable(gamescope_test gamescope_test.cpp)
target_link_libraries(gamescope_test PRIVATE iideck::session)
add_test(NAME gamescope COMMAND gamescope_test)

add_executable(nested_session_test nested_session_test.cpp)
target_link_libraries(nested_session_test PRIVATE iideck::session lucent::lucent Threads::Threads)
add_test(NAME nested_session COMMAND nested_session_test)
set_tests_properties(nested_session PROPERTIES TIMEOUT 120)
target_compile_definitions(nested_session_test PRIVATE IIDECK_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")

add_executable(game_keys_test game_keys_test.cpp)
target_link_libraries(game_keys_test PRIVATE iideck::session X11::X11 X11::Xtst)
add_test(NAME game_keys COMMAND game_keys_test)
set_tests_properties(game_keys PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
