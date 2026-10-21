add_library(opensu_session STATIC game_keys.cpp gamescope.cpp gamescope_overlay.cpp gamescope_windows.cpp steam_window_hiding.cpp window_focus.cpp monitor.cpp compositor_session.cpp restart_budget.cpp login_session.cpp)
add_library(opensu::session ALIAS opensu_session)
target_include_directories(opensu_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_session
    PUBLIC opensu::launch opensu::host opensu::input lucent::lucent
    PRIVATE X11::X11 X11::Xi PkgConfig::SDL3
)
target_compile_features(opensu_session PUBLIC cxx_std_20)
