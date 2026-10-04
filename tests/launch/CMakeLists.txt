add_executable(handoff_test handoff_test.cpp)
target_link_libraries(handoff_test PRIVATE iideck::launch iideck::library lucent::lucent)
add_test(NAME handoff COMMAND handoff_test)

# The "did not start" case waits out the real three-minute appearance bound, so
# it is a separate test run on request rather than three minutes in every run.
add_test(NAME handoff_slow COMMAND handoff_test --slow)
set_tests_properties(handoff_slow PROPERTIES DISABLED TRUE LABELS slow)
