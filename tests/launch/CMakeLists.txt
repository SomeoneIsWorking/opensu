add_executable(handoff_test handoff_test.cpp)
target_link_libraries(handoff_test PRIVATE iideck::launch iideck::library lucent::lucent Threads::Threads)
add_test(NAME handoff COMMAND handoff_test)
set_tests_properties(handoff PROPERTIES TIMEOUT 240)

add_executable(instance_test instance_test.cpp)
target_link_libraries(instance_test PRIVATE iideck::launch lucent::lucent Threads::Threads)
add_test(NAME instance COMMAND instance_test)
set_tests_properties(instance PROPERTIES TIMEOUT 120)

add_executable(process_tree_test process_tree_test.cpp)
target_link_libraries(process_tree_test PRIVATE iideck::launch lucent::lucent Threads::Threads)
add_test(NAME process_tree COMMAND process_tree_test)
set_tests_properties(process_tree PROPERTIES TIMEOUT 60)

add_executable(command_test command_test.cpp)
target_link_libraries(command_test PRIVATE iideck::launch lucent::lucent Threads::Threads)
add_test(NAME command COMMAND command_test)
set_tests_properties(command PROPERTIES TIMEOUT 60)

foreach(test handoff_test instance_test process_tree_test command_test)
    target_compile_definitions(${test} PRIVATE IIDECK_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
endforeach()
