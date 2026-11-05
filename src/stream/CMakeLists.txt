# Remote play's sessions: the host's and the guest's ends of a stream of sealed UDP datagrams
# (`packet_seal`, `stream_wire`) and the link they measure (`link_rate`; `link_cap`, a test run's
# slow link); what the host's Gamescope shows (`screen_capture`, PipeWire) coded with PyroWave or a
# hardware codec through VA-API (`video_codec`); the game's sound from a sink of its own
# (`game_sound`) coded with Opus (`audio_codec`); and the guest's pad, keys and pointer played into
# the game (`host_input`). Linux builds all of it; Windows and macOS build the guest's end, and their
# host's parts refuse; Android builds neither (`remote_reach` says so to the owners that offer it).
# Sealed datagrams on a UDP socket, apart so the runner's netplay takes them without the rest.
add_library(opensu_datagram STATIC packet_seal.cpp)
add_library(opensu::datagram ALIAS opensu_datagram)
target_include_directories(opensu_datagram PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_datagram PRIVATE opensu::secrets opensu::fileio)
target_compile_features(opensu_datagram PUBLIC cxx_std_20)
opensu_warnings(opensu_datagram)
if(ANDROID)
    target_sources(opensu_datagram PRIVATE udp_socket_none.cpp)
elseif(WIN32)
    target_sources(opensu_datagram PRIVATE udp_socket_winsock.cpp)
    target_link_libraries(opensu_datagram PRIVATE ws2_32)
else()
    target_sources(opensu_datagram PRIVATE udp_socket_posix.cpp)
endif()

add_library(opensu_stream STATIC stream_wire.cpp link_rate.cpp link_cap.cpp token_bucket.cpp
    host_stream.cpp audience.cpp guest_stream.cpp remote_reach.cpp video_codec.cpp picture_colour.cpp
    host_video.cpp guest_video.cpp host_audio.cpp guest_audio.cpp)
add_library(opensu::stream ALIAS opensu_stream)
target_include_directories(opensu_stream PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_stream
    PUBLIC opensu::datagram opensu::lan opensu::pad lucent::lucent gpu2d::render
    PRIVATE opensu::secrets opensu::clock)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(PIPEWIRE IMPORTED_TARGET libpipewire-0.3)
    if(NOT PIPEWIRE_FOUND)
        opensu_missing_package("openSU needs libpipewire 0.3 to capture the screen remote play streams"
            FEDORA pipewire-devel DEBIAN libpipewire-0.3-dev)
    endif()
    pkg_check_modules(XTST IMPORTED_TARGET xtst)
    if(NOT XTST_FOUND)
        opensu_missing_package("openSU needs libXtst to play a remote player's keys and pointer"
            FEDORA libXtst-devel DEBIAN libxtst-dev)
    endif()
    target_sources(opensu_stream PRIVATE pipewire_session.cpp screen_capture_pipewire.cpp
        game_sound_pipewire.cpp host_input.cpp)
    target_link_libraries(opensu_stream PRIVATE PkgConfig::PIPEWIRE PkgConfig::XTST)
else()
    target_sources(opensu_stream PRIVATE screen_capture_none.cpp game_sound_none.cpp
        host_input_none.cpp)
endif()
if(ANDROID)
    target_sources(opensu_stream PRIVATE audio_codec_none.cpp)
else()
    pkg_check_modules(OPUS IMPORTED_TARGET opus)
    if(NOT OPUS_FOUND)
        opensu_missing_package("openSU needs libopus to carry a game's sound to a remote player"
            FEDORA opus-devel DEBIAN libopus-dev)
    endif()
    target_sources(opensu_stream PRIVATE audio_codec_opus.cpp)
    target_link_libraries(opensu_stream PRIVATE PkgConfig::OPUS)
endif()
# The hardware codecs come from the system's libavcodec, whose library version differs between
# distributions, so a package built on one and run on another builds without them.
option(OPENSU_BUILD_VAAPI
    "Code remote play's video in hardware through the system's libavcodec and VA-API when the link \
is too slow for PyroWave. OFF builds remote play with PyroWave alone" ON)
if(OPENSU_BUILD_VAAPI AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(AVCODEC IMPORTED_TARGET libavcodec libavutil libva)
    if(NOT AVCODEC_FOUND)
        opensu_missing_package("openSU needs libavcodec and libva for remote play's hardware video codecs"
            FEDORA "libavcodec-free-devel libavutil-free-devel libva-devel"
            DEBIAN "libavcodec-dev libavutil-dev libva-dev"
            OTHERWISE "configure with -DOPENSU_BUILD_VAAPI=OFF")
    endif()
    target_sources(opensu_stream PRIVATE hardware_codec_vaapi.cpp)
    target_link_libraries(opensu_stream PRIVATE PkgConfig::AVCODEC)
else()
    target_sources(opensu_stream PRIVATE hardware_codec_none.cpp)
endif()
if(TARGET opensu::pyrowave)
    target_sources(opensu_stream PRIVATE pyrowave_codec.cpp)
    target_link_libraries(opensu_stream PRIVATE opensu::pyrowave opensu::vulkan)
    add_dependencies(opensu_stream opensu_pyrowave)
else()
    target_sources(opensu_stream PRIVATE pyrowave_codec_none.cpp)
endif()
target_compile_features(opensu_stream PUBLIC cxx_std_20)
opensu_warnings(opensu_stream)
