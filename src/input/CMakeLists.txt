# Which device the player last used, and the keys the shell binds. Pure apart from raylib's key
# codes, so tests link it alone.
add_library(opensu_input STATIC last_device.cpp keyboard_bindings.cpp key_names.cpp shortcuts.cpp pad_chords.cpp)
add_library(opensu::input ALIAS opensu_input)
target_include_directories(opensu_input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_input PUBLIC opensu::pad opensu::raylib)
target_compile_features(opensu_input PUBLIC cxx_std_20)
target_compile_options(opensu_input PRIVATE -Wall -Wextra -Wpedantic)
