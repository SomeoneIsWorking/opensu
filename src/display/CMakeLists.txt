# The display as the shell sees it on every OS: outputs, whose window has the focus, what the overlay
# shows. No window-system code; `src/session` reads these from Gamescope on Linux.
add_library(opensu_display STATIC output.cpp focus.cpp)
add_library(opensu::display ALIAS opensu_display)
target_include_directories(opensu_display PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_display PUBLIC opensu::command PRIVATE PkgConfig::SDL3 lucent::lucent)
target_compile_features(opensu_display PUBLIC cxx_std_20)
