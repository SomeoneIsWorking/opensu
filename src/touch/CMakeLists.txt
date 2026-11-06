# The on-screen pad, shared by the runner's window and the shell's over a remote game.
add_library(opensu_touch STATIC touch_layer.cpp touch_art.cpp pad_layout.cpp sdl_touch.cpp)
add_library(opensu::touch ALIAS opensu_touch)
target_include_directories(opensu_touch PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_touch PUBLIC opensu::pad PkgConfig::SDL3 PRIVATE opensu::svg)
target_compile_features(opensu_touch PUBLIC cxx_std_20)
opensu_warnings(opensu_touch)
