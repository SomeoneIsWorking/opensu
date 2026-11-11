add_executable(letterbox_test letterbox_test.cpp)
target_link_libraries(letterbox_test PRIVATE opensu::display)
opensu_warnings(letterbox_test)
add_test(NAME letterbox COMMAND letterbox_test)

add_executable(window_size_test window_size_test.cpp)
target_link_libraries(window_size_test PRIVATE opensu::display)
opensu_warnings(window_size_test)
add_test(NAME window_size COMMAND window_size_test)

add_executable(recommended_scale_test recommended_scale_test.cpp)
target_link_libraries(recommended_scale_test PRIVATE opensu::display)
opensu_warnings(recommended_scale_test)
add_test(NAME recommended_scale COMMAND recommended_scale_test)
