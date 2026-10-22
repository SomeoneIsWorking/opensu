# Controllers: the shell's controls, the pads, and key repeat. The pads come from this OS's
# `PadSource`: evdev on Linux, with uinput virtual pads a game reads while they are held; SDL's
# gamepads elsewhere. The SDL backend is portable, so it is compiled on Linux too.
add_library(opensu_pad STATIC
    event.cpp chord_gate.cpp forward_map.cpp direction_repeat.cpp sdl_pads.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(opensu_pad PRIVATE pad_translator.cpp evdev_device.cpp virtual_pad.cpp evdev_pads.cpp
        pad_source_evdev.cpp)
else()
    target_sources(opensu_pad PRIVATE pad_source_sdl.cpp)
endif()
add_library(opensu::pad ALIAS opensu_pad)
target_include_directories(opensu_pad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_pad PUBLIC PkgConfig::SDL3 PRIVATE lucent::lucent)
target_compile_features(opensu_pad PUBLIC cxx_std_20)
target_compile_options(opensu_pad PRIVATE -Wall -Wextra -Wpedantic)
