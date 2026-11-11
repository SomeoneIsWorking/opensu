# The display as the shell sees it on every OS: outputs, the scale a window is drawn at, whose
# window has the focus, what the overlay shows. No window-system code; `src/session` reads these
# from Gamescope on Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OPENSU_DISPLAY_PANEL panel_drm.cpp)
else()
    set(OPENSU_DISPLAY_PANEL panel_none.cpp)
endif()
add_library(opensu_display STATIC output.cpp focus.cpp scale.cpp panel.cpp ${OPENSU_DISPLAY_PANEL})
add_library(opensu::display ALIAS opensu_display)
target_include_directories(opensu_display PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_display PUBLIC opensu::command PkgConfig::SDL3 PRIVATE lucent::lucent)
target_compile_features(opensu_display PUBLIC cxx_std_20)
