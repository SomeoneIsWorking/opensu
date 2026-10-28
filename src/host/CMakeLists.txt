# Host services behind seams: Bluetooth (BlueZ), power (logind) and the backlight. Each is an
# interface plus a real backend that runs the system's own tools through a `Runner`.
add_library(opensu_host STATIC runner.cpp bluetooth.cpp desktop_request.cpp login_session_end.cpp power.cpp
            session_mode.cpp backlight.cpp desktop_night_light.cpp)
add_library(opensu::host ALIAS opensu_host)
target_link_libraries(opensu_host PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" INTERFACE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_host PUBLIC opensu::command PRIVATE opensu::fileio lucent::lucent)
target_compile_features(opensu_host PUBLIC cxx_std_20)
target_compile_options(opensu_host PRIVATE -Wall -Wextra -Wpedantic)

