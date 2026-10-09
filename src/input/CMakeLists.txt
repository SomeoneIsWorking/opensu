# Which device the player last used, and the keys the shell binds. Pure apart from raylib's key
# codes, so tests link it alone.
add_library(iideck_input STATIC last_device.cpp keyboard_bindings.cpp)
add_library(iideck::input ALIAS iideck_input)
target_include_directories(iideck_input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_input PUBLIC iideck::pad iideck::raylib)
target_compile_features(iideck_input PUBLIC cxx_std_20)
target_compile_options(iideck_input PRIVATE -Wall -Wextra -Wpedantic)
