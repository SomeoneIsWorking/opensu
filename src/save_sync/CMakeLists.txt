# The saves of the games installed here shared with targets: paired openSUs (their target is the
# LAN's), a folder or a WebDAV share sealed with the player's passphrase; the record of what was
# synced and the bundle saves travel in.
add_library(opensu_save_sync STATIC save_versions.cpp save_bundle.cpp save_sync.cpp
            folder_share.cpp webdav_share.cpp share_target.cpp)
add_library(opensu::save_sync ALIAS opensu_save_sync)
target_include_directories(opensu_save_sync PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_save_sync
    PUBLIC opensu::library opensu::net opensu::secrets lucent::lucent nlohmann_json::nlohmann_json
    PRIVATE opensu::clock opensu::fileio)
target_compile_features(opensu_save_sync PUBLIC cxx_std_20)
opensu_warnings(opensu_save_sync)
