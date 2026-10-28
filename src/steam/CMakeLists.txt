add_library(opensu_steam STATIC client.cpp devtools.cpp downloads.cpp download_control.cpp steam_json.cpp
    install_wizard.cpp uninstaller.cpp big_picture.cpp launch_activity.cpp orphaned_steam.cpp libraries.cpp sandbox.cpp secret_bus.cpp
    steamgriddb_sign_in.cpp store_page.cpp)
add_library(opensu::steam ALIAS opensu_steam)
target_link_libraries(opensu_steam PUBLIC nlohmann_json::nlohmann_json)
# The client's modes: openSU's own scoped and sandboxed Steam exist only on Linux; Android has none.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(opensu_steam PRIVATE client_modes_scoped.cpp)
elseif(ANDROID)
    target_sources(opensu_steam PRIVATE client_modes_none.cpp)
else()
    target_sources(opensu_steam PRIVATE client_modes_normal.cpp)
endif()
# Where a running client is found: its pid file and /proc, or the Windows registry.
if(WIN32)
    target_sources(opensu_steam PRIVATE desktop_steam_registry.cpp)
else()
    target_sources(opensu_steam PRIVATE desktop_steam_pid_file.cpp)
endif()
target_include_directories(opensu_steam PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_steam PUBLIC opensu::launch opensu::library lucent::lucent Threads::Threads)
target_compile_features(opensu_steam PUBLIC cxx_std_20)
