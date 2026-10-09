add_library(opensu_steam STATIC client.cpp desktop_steam.cpp devtools.cpp downloads.cpp
    install_wizard.cpp launch_activity.cpp)
add_library(opensu::steam ALIAS opensu_steam)
target_include_directories(opensu_steam PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_steam PUBLIC opensu::launch opensu::library lucent::lucent Threads::Threads)
target_compile_features(opensu_steam PUBLIC cxx_std_20)
