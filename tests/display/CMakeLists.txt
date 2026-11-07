add_executable(letterbox_test letterbox_test.cpp)
target_link_libraries(letterbox_test PRIVATE opensu::display)
opensu_warnings(letterbox_test)
add_test(NAME letterbox COMMAND letterbox_test)
