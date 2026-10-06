add_executable(handoff_test handoff_test.cpp)
target_link_libraries(handoff_test PRIVATE iideck::launch iideck::library lucent::lucent Threads::Threads)
add_test(NAME handoff COMMAND handoff_test)
set_tests_properties(handoff PROPERTIES TIMEOUT 180)

add_executable(instance_test instance_test.cpp)
target_link_libraries(instance_test PRIVATE iideck::launch lucent::lucent Threads::Threads)
add_test(NAME instance COMMAND instance_test)
set_tests_properties(instance PROPERTIES TIMEOUT 120)

add_executable(gamescope_test gamescope_test.cpp)
target_link_libraries(gamescope_test PRIVATE iideck::launch)
add_test(NAME gamescope COMMAND gamescope_test)

add_executable(desktop_steam_test desktop_steam_test.cpp)
target_link_libraries(desktop_steam_test PRIVATE iideck::launch)
add_test(NAME desktop_steam COMMAND desktop_steam_test)

foreach(test handoff_test instance_test desktop_steam_test)
    target_compile_definitions(${test} PRIVATE IIDECK_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
endforeach()
