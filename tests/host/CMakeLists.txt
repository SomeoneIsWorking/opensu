add_executable(bluetooth_test bluetooth_test.cpp)
target_link_libraries(bluetooth_test PRIVATE opensu::host)
target_include_directories(bluetooth_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(bluetooth_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME bluetooth COMMAND bluetooth_test)

add_executable(power_test power_test.cpp)
target_link_libraries(power_test PRIVATE opensu::host)
target_include_directories(power_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(power_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME power COMMAND power_test)

add_executable(backlight_test backlight_test.cpp)
target_link_libraries(backlight_test PRIVATE opensu::host)
target_include_directories(backlight_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(backlight_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(backlight_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME backlight COMMAND backlight_test)

add_executable(desktop_request_test desktop_request_test.cpp)
target_link_libraries(desktop_request_test PRIVATE opensu::host)
target_compile_options(desktop_request_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(desktop_request_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME desktop_request COMMAND desktop_request_test)

add_executable(session_mode_test session_mode_test.cpp)
target_link_libraries(session_mode_test PRIVATE opensu::host)
target_include_directories(session_mode_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(session_mode_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(session_mode_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME session_mode COMMAND session_mode_test)

# The root-run scripts, against a temp root; a root caller would ignore the root, so it is skipped.
add_executable(session_scripts_test session_scripts_test.cpp)
target_link_libraries(session_scripts_test PRIVATE opensu::host)
target_include_directories(session_scripts_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(session_scripts_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(session_scripts_test PRIVATE
    OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch"
    OPENSU_PACKAGING_DIR="${CMAKE_SOURCE_DIR}/packaging")
add_test(NAME session_scripts COMMAND session_scripts_test)
set_tests_properties(session_scripts PROPERTIES SKIP_RETURN_CODE 77)

add_executable(login_session_end_test login_session_end_test.cpp)
target_link_libraries(login_session_end_test PRIVATE opensu::host)
target_include_directories(login_session_end_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(login_session_end_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(login_session_end_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME login_session_end COMMAND login_session_end_test)
