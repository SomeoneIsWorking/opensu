# Netplay whichever core plays: a session's players, frames, input and end (`session`) over a
# transport (`transport`): sealed UDP (`udp_transport`).
add_library(opensu_netplay STATIC session.cpp udp_transport.cpp)
add_library(opensu::netplay ALIAS opensu_netplay)
target_include_directories(opensu_netplay PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_netplay PUBLIC opensu::datagram PRIVATE opensu::fileio lucent::lucent)
target_compile_features(opensu_netplay PUBLIC cxx_std_20)
opensu_warnings(opensu_netplay)
