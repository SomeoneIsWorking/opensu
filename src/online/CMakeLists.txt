# Two people playing over the internet with no server: Lucent's `lucent::online` takes a code to an
# authenticated path (`p2p::Connector`), and `meeting` agrees on a game over it. The shell's alone:
# the game runs over `netplay::UdpTransport`.
add_library(opensu_online STATIC meeting.cpp)
add_library(opensu::online ALIAS opensu_online)
target_include_directories(opensu_online PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_online
    PUBLIC opensu::lan lucent::online
    PRIVATE opensu::fileio)
target_compile_features(opensu_online PUBLIC cxx_std_20)
opensu_warnings(opensu_online)
