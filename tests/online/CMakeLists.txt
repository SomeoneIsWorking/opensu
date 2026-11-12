# Two meetings over loopback with a stand-in STUN server, through the shipping code. The transport
# under them is Lucent's and tested there (`lucent/tests/p2p`).
add_executable(online_meeting_test meeting_test.cpp)
target_link_libraries(online_meeting_test PRIVATE opensu::online lucent::lucent)
target_include_directories(online_meeting_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
opensu_warnings(online_meeting_test)
add_test(NAME online_meeting COMMAND online_meeting_test)

# A public game listed, asked for and taken down through Lucent's lab relay on loopback.
add_executable(online_lobbies_test lobbies_test.cpp)
target_link_libraries(online_lobbies_test PRIVATE opensu::online lucent::lucent)
target_include_directories(online_lobbies_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
opensu_warnings(online_lobbies_test)
add_test(NAME online_lobbies COMMAND online_lobbies_test)
set_tests_properties(online_lobbies PROPERTIES TIMEOUT 120)

# Lucent's lab root is a host tool, so Android builds have no meeting test.
if(OPENSU_BUILD_ZEROTIER AND TARGET lucent_zerotier_root)
    # Two openSUs meet over the embedded node alone through a lab root at an address of their own.
    add_executable(online_zero_tier_meeting_test zero_tier_meeting_test.cpp)
    target_link_libraries(online_zero_tier_meeting_test
        PRIVATE opensu::online opensu::launch lucent::lucent)
    target_include_directories(online_zero_tier_meeting_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_compile_definitions(online_zero_tier_meeting_test PRIVATE
        OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch"
        OPENSU_ZERO_TIER_ROOT="$<TARGET_FILE:lucent_zerotier_root>")
    # Lucent's lab root, which tools.nat_lab runs too.
    add_dependencies(online_zero_tier_meeting_test lucent_zerotier_root)
    opensu_warnings(online_zero_tier_meeting_test)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # libzt binds no lo, tun or tap interface, and CI's container has only slirp4netns' tap0:
        # a network namespace of the test's own with a dummy lab0.
        add_test(NAME online_zero_tier_meeting
            COMMAND unshare --map-root-user --net sh -c
                "ip link set lo up && ip link add lab0 type dummy && ip addr add 10.77.0.1/24 dev lab0 && ip link set lab0 up && exec \"$0\""
                $<TARGET_FILE:online_zero_tier_meeting_test>)
    else()
        add_test(NAME online_zero_tier_meeting COMMAND online_zero_tier_meeting_test)
    endif()
    set_tests_properties(online_zero_tier_meeting PROPERTIES TIMEOUT 400)
endif()
