# opensu-retro, the built-in libretro runner: one core per process, an SDL window drawn with Vulkan.

add_library(opensu_retro STATIC
    core.cpp core_environment.cpp core_options.cpp video_frame.cpp audio_sink.cpp
    audio_output.cpp joypad.cpp save_memory.cpp vulkan_device.cpp frame_filter.cpp
    frame_compositor.cpp presenter.cpp save_state.cpp control_server.cpp runner_app.cpp)
gpu2d_compile_shaders(opensu_retro shaders/frame.vert shaders/frame.frag shaders/sharp_bilinear.frag
    shaders/crt_lottes.frag)
add_library(opensu::retro ALIAS opensu_retro)
target_include_directories(opensu_retro
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_retro
    PUBLIC opensu::libretro_header opensu::pad opensu::fileio opensu::setting_layers lucent::lucent
           PkgConfig::SDL3 PkgConfig::VULKAN
    PRIVATE gpu2d::image ${CMAKE_DL_LIBS})
target_compile_features(opensu_retro PUBLIC cxx_std_20)
target_compile_options(opensu_retro PRIVATE -Wall -Wextra -Wpedantic)

add_executable(opensu-retro main.cpp)
target_link_libraries(opensu-retro PRIVATE opensu::retro opensu::config lucent::lucent)
target_compile_options(opensu-retro PRIVATE -Wall -Wextra -Wpedantic)
# Beside opensu, in the build tree as installed, where the shell looks for it.
set_target_properties(opensu-retro PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src")
install(TARGETS opensu-retro RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
