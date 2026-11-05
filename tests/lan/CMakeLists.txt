add_executable(lan_node_test lan_node_test.cpp)
target_link_libraries(lan_node_test PRIVATE opensu::lan opensu::command)
opensu_warnings(lan_node_test)
target_compile_definitions(lan_node_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME lan_node COMMAND lan_node_test)
# Two nodes paired through the system's own DNS-SD daemon: mDNSResponder on macOS, Windows' DNS
# client. Linux's CI image runs no Avahi; there the product's hidden pair checks it.
if(APPLE OR WIN32)
    add_test(NAME lan_discovery COMMAND lan_node_test --system-discovery)
endif()
