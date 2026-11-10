# opensu-retro, the built-in libretro runner: one core per process, an SDL window drawn with Vulkan.

# Where the runner keeps a game's saves and the resume state's format, which the shell reads too.
add_library(opensu_retro_saves STATIC save_paths.cpp resume_file.cpp)
add_library(opensu::retro_saves ALIAS opensu_retro_saves)
target_include_directories(opensu_retro_saves PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_retro_saves PRIVATE nlohmann_json::nlohmann_json opensu::fileio opensu::zip)
target_compile_features(opensu_retro_saves PUBLIC cxx_std_20)
opensu_warnings(opensu_retro_saves)

add_library(opensu_retro STATIC
    core.cpp core_environment.cpp core_options.cpp video_frame.cpp audio_sink.cpp
    audio_output.cpp joypad.cpp save_memory.cpp save_ram_fit.cpp vulkan_device.cpp frame_filter.cpp
    shared_library.cpp librashader_library.cpp shader_chain.cpp shader_scale.cpp gpu_timer.cpp
    reduced_output.cpp streamed_texture.cpp fullscreen_pipeline.cpp frame_compositor.cpp frame_art.cpp sdl_runner_window.cpp presenter.cpp hardware_render.cpp
    save_state.cpp resume_state.cpp control_server.cpp control_commands.cpp overlay_pass.cpp frame_stages.cpp
    netplay_link.cpp arcade_cabinet.cpp runner_touch.cpp screen_turn.cpp runner_app.cpp)
gpu2d_compile_shaders(opensu_retro shaders/frame.vert shaders/frame.frag shaders/sharp_bilinear.frag
    shaders/crt_lottes.frag shaders/sampled.frag)
add_library(opensu::retro ALIAS opensu_retro)
target_link_libraries(opensu_retro PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_retro
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_retro
    PUBLIC opensu::retro_saves opensu::libretro_header opensu::librashader_header opensu::pad opensu::command opensu::fileio opensu::setting_layers opensu::netplay lucent::lucent
           PkgConfig::SDL3 opensu::vulkan opensu::touch opensu::input
    PRIVATE gpu2d::image opensu::zip opensu::secrets)
# Where the shell cannot draw over the runner (Android), Guide and Back bring the shell's Activity
# forward and the runner draws its own touch controls.
if(ANDROID)
    target_sources(opensu_retro PRIVATE shell_handoff_android.cpp hosted_runner_window_android.cpp)
    target_compile_definitions(opensu_retro PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
    target_link_libraries(opensu_retro PRIVATE opensu::android android)
else()
    target_sources(opensu_retro PRIVATE shell_handoff_none.cpp hosted_runner_window_none.cpp)
endif()
target_compile_features(opensu_retro PUBLIC cxx_std_20)
opensu_warnings(opensu_retro)

# On Android the runner is libopensu-retro.so, which `:retro` loads, in its Activity or Service.
if(ANDROID)
    add_library(opensu-retro SHARED main.cpp)
else()
    add_executable(opensu-retro main.cpp)
endif()
target_link_libraries(opensu-retro PRIVATE opensu::retro opensu::config lucent::lucent)
opensu_warnings(opensu-retro)
opensu_windowed(opensu-retro)
# Beside opensu, in the build tree as installed, where the shell looks for it.
set_target_properties(opensu-retro PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src")
# The librashader it loads for a shader preset, staged where `Config::librashader` looks.
if(TARGET opensu_librashader)
    add_dependencies(opensu-retro opensu_librashader)
endif()
install(TARGETS opensu-retro RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
