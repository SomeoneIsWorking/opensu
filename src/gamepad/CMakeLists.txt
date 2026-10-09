# Controllers through evdev and uinput: the shell's controls, the pads, the virtual pads a game
# reads while they are held, and key repeat.
add_library(opensu_pad STATIC
    event.cpp direction_repeat.cpp pad_translator.cpp evdev_device.cpp virtual_pad.cpp pads.cpp)
add_library(opensu::pad ALIAS opensu_pad)
target_include_directories(opensu_pad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_pad PRIVATE lucent::lucent)
target_compile_features(opensu_pad PUBLIC cxx_std_20)
target_compile_options(opensu_pad PRIVATE -Wall -Wextra -Wpedantic)
