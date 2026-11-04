# Remote play's sessions: the host's and the guest's ends of a stream of sealed UDP datagrams
# (`packet_seal`, `stream_wire`); what the host's Gamescope shows (`screen_capture`, PipeWire) coded
# with PyroWave (`video_codec`); the game's sound from a sink of its own (`game_sound`) coded with
# Opus (`audio_codec`); and the guest's pad, keys and pointer played into the game (`host_input`).
# Linux builds all of it; elsewhere each of those refuses, and so does a session.
add_library(opensu_stream STATIC packet_seal.cpp stream_wire.cpp host_stream.cpp guest_stream.cpp
    picture_colour.cpp host_video.cpp guest_video.cpp host_audio.cpp guest_audio.cpp)
add_library(opensu::stream ALIAS opensu_stream)
target_include_directories(opensu_stream PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_stream
    PUBLIC opensu::lan lucent::lucent
    PRIVATE opensu::secrets opensu::clock)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
    if(NOT PIPEWIRE_FOUND)
        message(FATAL_ERROR
            "openSU needs libpipewire 0.3 to capture the screen remote play streams. Install it "
            "with `sudo dnf install pipewire-devel` (Fedora) or "
            "`sudo apt install libpipewire-0.3-dev` (Debian/Ubuntu)")
    endif()
    pkg_check_modules(XTST IMPORTED_TARGET xtst)
    if(NOT XTST_FOUND)
        message(FATAL_ERROR
            "openSU needs libXtst to play a remote player's keys and pointer into the game. Install "
            "it with `sudo dnf install libXtst-devel` (Fedora) or `sudo apt install libxtst-dev` "
            "(Debian/Ubuntu)")
    endif()
    pkg_check_modules(OPUS IMPORTED_TARGET opus)
    if(NOT OPUS_FOUND)
        message(FATAL_ERROR
            "openSU needs libopus to send a game's sound to a remote player. Install it with "
            "`sudo dnf install opus-devel` (Fedora) or `sudo apt install libopus-dev` "
            "(Debian/Ubuntu)")
    endif()
    target_sources(opensu_stream PRIVATE udp_socket_posix.cpp pipewire_session.cpp
        screen_capture_pipewire.cpp game_sound_pipewire.cpp host_input.cpp audio_codec_opus.cpp)
    target_link_libraries(opensu_stream
        PRIVATE PkgConfig::PIPEWIRE PkgConfig::XTST PkgConfig::OPUS opensu::pad)
else()
    target_sources(opensu_stream PRIVATE udp_socket_none.cpp screen_capture_none.cpp
        game_sound_none.cpp host_input_none.cpp audio_codec_none.cpp)
endif()
if(TARGET opensu::pyrowave)
    target_sources(opensu_stream PRIVATE video_codec_pyrowave.cpp)
    target_link_libraries(opensu_stream PRIVATE opensu::pyrowave opensu::vulkan)
    add_dependencies(opensu_stream opensu_pyrowave)
else()
    target_sources(opensu_stream PRIVATE video_codec_none.cpp)
endif()
target_compile_features(opensu_stream PUBLIC cxx_std_20)
opensu_warnings(opensu_stream)
