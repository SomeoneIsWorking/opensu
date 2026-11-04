# This openSU on the LAN: its paired peers, the listener they reach it on, pairing, the games each
# shares and their saves, and its DNS-SD announcement. Finding other openSUs is Avahi over the system bus on Linux;
# elsewhere it is not built yet and says so.
add_library(opensu_lan STATIC peer_crypto.cpp pairing_keys.cpp request_signature.cpp peer_book.cpp
            wire.cpp shared_library.cpp remote_library.cpp peer_client.cpp host_pairing.cpp
            guest_pairing.cpp save_bundle.cpp save_versions.cpp save_sync.cpp lan_listener.cpp
            lan_node.cpp)
add_library(opensu::lan ALIAS opensu_lan)
target_include_directories(opensu_lan PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_lan
    PUBLIC opensu::library opensu::net opensu::secrets lucent::lucent
    PRIVATE opensu::clock opensu::fileio nlohmann_json::nlohmann_json OpenSSL::Crypto)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(LAN_LIBSYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
    target_sources(opensu_lan PRIVATE dns_sd_avahi.cpp)
    target_link_libraries(opensu_lan PRIVATE opensu::host PkgConfig::LAN_LIBSYSTEMD)
else()
    target_sources(opensu_lan PRIVATE dns_sd_none.cpp)
endif()
target_compile_features(opensu_lan PUBLIC cxx_std_20)
opensu_warnings(opensu_lan)
