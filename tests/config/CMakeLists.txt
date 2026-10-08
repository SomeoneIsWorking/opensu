add_executable(config_test config_test.cpp)
target_link_libraries(config_test PRIVATE iideck::config)
add_test(NAME config_session_default COMMAND config_test default)
add_test(NAME config_session_inherited COMMAND config_test inherited)
set_tests_properties(config_session_default PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_SESSION=unset:")
set_tests_properties(config_session_inherited PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_SESSION=set:iideck-test-named")

foreach(mode default standard wiisu invalid)
    add_test(NAME config_home_${mode} COMMAND config_test home-${mode})
endforeach()
set_tests_properties(config_home_default PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_HOME_MODE=unset:")
set_tests_properties(config_home_standard PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_HOME_MODE=set:standard")
set_tests_properties(config_home_wiisu PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_HOME_MODE=set:wiisu")
set_tests_properties(config_home_invalid PROPERTIES
    ENVIRONMENT_MODIFICATION "IIDECK_HOME_MODE=set:paged")

add_test(NAME config_time_format COMMAND config_test time-format)

add_test(NAME config_data_dir COMMAND config_test data-dir)
add_test(NAME config_data_dir_default COMMAND config_test data-dir-default)
set_tests_properties(config_data_dir PROPERTIES
    ENVIRONMENT_MODIFICATION "XDG_DATA_HOME=set:/xdg-data")
set_tests_properties(config_data_dir_default PROPERTIES
    ENVIRONMENT_MODIFICATION "XDG_DATA_HOME=unset:")

add_test(NAME config_gamescope COMMAND config_test gamescope)
