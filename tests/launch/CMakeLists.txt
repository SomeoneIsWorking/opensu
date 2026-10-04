add_executable(handoff_test handoff_test.cpp)
target_link_libraries(handoff_test PRIVATE iideck::launch iideck::library lucent::lucent Threads::Threads)
add_test(NAME handoff COMMAND handoff_test)
set_tests_properties(handoff PROPERTIES TIMEOUT 180)
