# Two people playing over the internet with no server: a code carries what a server would
# (`invite_code`), STUN and the router say where this machine can be reached (`stun`, `nat_pmp`,
# `upnp_igd`, `port_lease`), the machines find each other (`rendezvous`), and two shells agree on a
# game (`reliable_link`, `meeting`). The shell's alone: the game runs over `netplay::UdpTransport`.
add_library(opensu_online STATIC stun.cpp invite_code.cpp host_network.cpp nat_pmp.cpp upnp_igd.cpp
    port_lease.cpp rendezvous.cpp reliable_link.cpp meeting.cpp)
add_library(opensu::online ALIAS opensu_online)
target_include_directories(opensu_online PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_online
    PUBLIC opensu::datagram opensu::secrets opensu::net opensu::lan
    PRIVATE opensu::fileio lucent::lucent)
target_compile_features(opensu_online PUBLIC cxx_std_20)
opensu_warnings(opensu_online)
if(ANDROID)
    target_sources(opensu_online PRIVATE host_network_none.cpp)
elseif(WIN32)
    target_sources(opensu_online PRIVATE host_network_windows.cpp)
    target_link_libraries(opensu_online PRIVATE ws2_32 iphlpapi)
else()
    target_sources(opensu_online PRIVATE host_network_posix.cpp)
endif()
