add_executable(config_test config_test.cpp)
target_link_libraries(config_test PRIVATE iideck::config)
add_test(NAME config_session_default COMMAND config_test default)
add_test(NAME config_session_inherited COMMAND config_test inherited)
set_tests_properties(config_session_default PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_SESSION=unset:")
set_tests_properties(config_session_inherited PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_SESSION=set:iideck-test-named")
