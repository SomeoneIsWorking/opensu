# The session reads X11 input and window properties and makes input through XTest.
find_package(X11 REQUIRED)
if(NOT X11_Xtst_FOUND)
    opensu_missing_package("openSU needs libXtst to type and point into the game in front"
        FEDORA libXtst-devel DEBIAN libxtst-dev)
endif()
add_library(opensu_session STATIC game_keys.cpp gamescope.cpp night_mode.cpp gamescope_overlay.cpp gamescope_windows.cpp steam_window_hiding.cpp root_properties.cpp window_focus.cpp monitor.cpp compositor_session.cpp restart_budget.cpp login_session.cpp xtest_input.cpp)
add_library(opensu::session ALIAS opensu_session)
target_include_directories(opensu_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_session
    PUBLIC opensu::display opensu::launch opensu::host opensu::input lucent::lucent
    PRIVATE X11::X11 X11::Xi X11::Xtst PkgConfig::SDL3
)
target_compile_features(opensu_session PUBLIC cxx_std_20)
