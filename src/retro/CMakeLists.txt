# opensu-retro, the built-in libretro runner: one core per process, an SDL window drawn with Vulkan.

add_library(opensu_retro STATIC
    core.cpp core_environment.cpp core_options.cpp video_frame.cpp audio_sink.cpp
    audio_output.cpp joypad.cpp save_memory.cpp vulkan_device.cpp frame_filter.cpp
    shared_library.cpp librashader_library.cpp shader_chain.cpp frame_compositor.cpp presenter.cpp hardware_render.cpp
    save_state.cpp control_server.cpp runner_app.cpp)
gpu2d_compile_shaders(opensu_retro shaders/frame.vert shaders/frame.frag shaders/sharp_bilinear.frag
    shaders/crt_lottes.frag)
add_library(opensu::retro ALIAS opensu_retro)
target_link_libraries(opensu_retro PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(opensu_retro
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_retro
    PUBLIC opensu::libretro_header opensu::librashader_header opensu::pad opensu::command opensu::fileio opensu::setting_layers lucent::lucent
           PkgConfig::SDL3 opensu::vulkan
    PRIVATE gpu2d::image)
target_compile_features(opensu_retro PUBLIC cxx_std_20)
opensu_warnings(opensu_retro)

# On Android the runner is libopensu-retro.so, which the `:retro` Activity loads.
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
