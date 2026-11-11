# Two meetings over loopback with a stand-in STUN server, through the shipping code. The transport
# under them is Lucent's and tested there (`lucent/tests/p2p`).
add_executable(online_meeting_test meeting_test.cpp)
target_link_libraries(online_meeting_test PRIVATE opensu::online lucent::lucent)
target_include_directories(online_meeting_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
opensu_warnings(online_meeting_test)
add_test(NAME online_meeting COMMAND online_meeting_test)
