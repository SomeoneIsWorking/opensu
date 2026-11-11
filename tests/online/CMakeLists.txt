# Two meetings over loopback with a stand-in STUN server, through the shipping code. The transport
# under them is Lucent's and tested there (`lucent/tests/p2p`).
add_executable(online_meeting_test meeting_test.cpp)
target_link_libraries(online_meeting_test PRIVATE opensu::online lucent::lucent)
target_include_directories(online_meeting_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
opensu_warnings(online_meeting_test)
add_test(NAME online_meeting COMMAND online_meeting_test)

# tools.nat_lab's ZeroTier root, where its namespaces have no internet: a lab tool, not a test.
if(OPENSU_BUILD_ZEROTIER)
    add_executable(online_zero_tier_root zero_tier_root.cpp)
    target_link_libraries(online_zero_tier_root PRIVATE lucent::zerotier opensu::online)
    opensu_warnings(online_zero_tier_root)

    # Two openSUs meet over the embedded node alone, a root at this machine's own address.
    add_executable(online_zero_tier_meeting_test zero_tier_meeting_test.cpp)
    target_link_libraries(online_zero_tier_meeting_test
        PRIVATE opensu::online opensu::launch lucent::lucent)
    target_include_directories(online_zero_tier_meeting_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_compile_definitions(online_zero_tier_meeting_test PRIVATE
        OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch"
        OPENSU_ZERO_TIER_ROOT="$<TARGET_FILE:online_zero_tier_root>")
    add_dependencies(online_zero_tier_meeting_test online_zero_tier_root)
    opensu_warnings(online_zero_tier_meeting_test)
    add_test(NAME online_zero_tier_meeting COMMAND online_zero_tier_meeting_test)
    set_tests_properties(online_zero_tier_meeting PROPERTIES TIMEOUT 400)
endif()
