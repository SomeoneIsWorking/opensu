add_executable(interface_theme_test interface_theme_test.cpp)
target_link_libraries(interface_theme_test PRIVATE opensu::theme)
target_compile_definitions(interface_theme_test PRIVATE
    OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(interface_theme_test)
add_test(NAME interface_theme COMMAND interface_theme_test)
