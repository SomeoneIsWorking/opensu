# A netplay session through its shipping code over an in-memory link.
add_executable(netplay_session_test session_test.cpp)
target_link_libraries(netplay_session_test PRIVATE opensu::netplay)
target_include_directories(netplay_session_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
opensu_warnings(netplay_session_test)
add_test(NAME netplay_session COMMAND netplay_session_test)
