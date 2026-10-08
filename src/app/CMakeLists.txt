add_library(iideck_app STATIC shell_app.cpp control_channel.cpp install_job.cpp launcher_status.cpp sign_in.cpp)
add_library(iideck::app ALIAS iideck_app)
target_include_directories(iideck_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_app
    PUBLIC
        iideck::library
        iideck::artwork
        iideck::ui
        iideck::device
        iideck::pad
        iideck::launch
        iideck::steam
        iideck::session
        iideck::raylib
        lucent::lucent
)
target_compile_features(iideck_app PUBLIC cxx_std_20)