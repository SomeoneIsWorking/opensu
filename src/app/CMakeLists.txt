add_library(opensu_app STATIC shell_app.cpp control_channel.cpp install_job.cpp steam_install_job.cpp cli_install_job.cpp epic_install_job.cpp gog_install_job.cpp installs.cpp launcher_status.cpp sign_in.cpp)
add_library(opensu::app ALIAS opensu_app)
target_include_directories(opensu_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_app
    PUBLIC
        opensu::library
        opensu::artwork
        opensu::audio
        opensu::settings
        opensu::ui
        opensu::device
        opensu::pad
        opensu::launch
        opensu::steam
        opensu::session
        opensu::raylib
        lucent::lucent
)
target_compile_features(opensu_app PUBLIC cxx_std_20)