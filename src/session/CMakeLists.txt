add_library(opensu_session STATIC game_keys.cpp gamescope.cpp gamescope_overlay.cpp gamescope_windows.cpp monitor.cpp nested_session.cpp)
add_library(opensu::session ALIAS opensu_session)
target_include_directories(opensu_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_session
    PUBLIC opensu::launch lucent::lucent
    PRIVATE opensu::raylib X11::X11 X11::Xi
)
target_compile_features(opensu_session PUBLIC cxx_std_20)
