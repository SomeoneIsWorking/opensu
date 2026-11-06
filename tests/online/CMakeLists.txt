# The internet path through its shipping code: codes, STUN, the router's protocols against stand-ins,
# and two machines over loopback, with forged, replayed and oversized datagrams thrown at them.
foreach(name invite_code stun port_lease rendezvous reliable_link meeting)
    add_executable(online_${name}_test ${name}_test.cpp)
    target_link_libraries(online_${name}_test PRIVATE opensu::online lucent::lucent)
    target_include_directories(online_${name}_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/..")
    opensu_warnings(online_${name}_test)
    add_test(NAME online_${name} COMMAND online_${name}_test)
endforeach()
