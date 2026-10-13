add_executable(plugin_host_test plugin_host_test.cpp)
target_link_libraries(plugin_host_test PRIVATE opensu::plugin)
target_include_directories(plugin_host_test PRIVATE "${CMAKE_SOURCE_DIR}/tests" "${CMAKE_SOURCE_DIR}/src")
target_compile_options(plugin_host_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(plugin_host_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME plugin_host COMMAND plugin_host_test)

add_executable(eden_playtime_test eden_playtime_test.cpp)
target_link_libraries(eden_playtime_test PRIVATE opensu::plugin)
target_include_directories(eden_playtime_test PRIVATE "${CMAKE_SOURCE_DIR}/tests" "${CMAKE_SOURCE_DIR}/src")
target_compile_options(eden_playtime_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(eden_playtime_test PRIVATE
    OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch"
    OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
add_test(NAME eden_playtime COMMAND eden_playtime_test)

add_executable(manifest_test manifest_test.cpp)
target_link_libraries(manifest_test PRIVATE opensu::plugin)
target_include_directories(manifest_test PRIVATE "${CMAKE_SOURCE_DIR}/tests" "${CMAKE_SOURCE_DIR}/src")
target_compile_options(manifest_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME plugin_manifest COMMAND manifest_test)
