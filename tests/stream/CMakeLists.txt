add_executable(stream_test stream_test.cpp)
target_link_libraries(stream_test PRIVATE opensu::stream)
opensu_warnings(stream_test)
add_test(NAME stream COMMAND stream_test)
