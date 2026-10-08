# Controller input without raylib: the shared controls, evdev devices, uinput virtual pads and the
# guard that holds the pads while a game runs.
add_library(iideck_pad STATIC event.cpp)
add_library(iideck::pad ALIAS iideck_pad)
target_include_directories(iideck_pad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_pad PRIVATE lucent::lucent)
target_compile_features(iideck_pad PUBLIC cxx_std_20)
target_compile_options(iideck_pad PRIVATE -Wall -Wextra -Wpedantic)

# The shell's controller reader, through raylib.
add_library(iideck_gamepad STATIC reader.cpp)
add_library(iideck::gamepad ALIAS iideck_gamepad)
target_include_directories(iideck_gamepad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_gamepad
    PUBLIC iideck::pad iideck::raylib
    PRIVATE lucent::lucent
)
target_compile_features(iideck_gamepad PUBLIC cxx_std_20)
