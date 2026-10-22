add_library(opensu_steam STATIC client.cpp desktop_steam.cpp devtools.cpp downloads.cpp download_control.cpp steam_json.cpp
    install_wizard.cpp uninstaller.cpp big_picture.cpp launch_activity.cpp orphaned_steam.cpp libraries.cpp sandbox.cpp secret_bus.cpp)
add_library(opensu::steam ALIAS opensu_steam)
# The client's modes: openSU's own scoped and sandboxed Steam exist only on Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(opensu_steam PRIVATE client_modes_scoped.cpp)
else()
    target_sources(opensu_steam PRIVATE client_modes_normal.cpp)
endif()
target_include_directories(opensu_steam PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_steam PUBLIC opensu::launch opensu::library lucent::lucent Threads::Threads)
target_compile_features(opensu_steam PUBLIC cxx_std_20)
