# opensu-retro, the built-in libretro runner: one core per process, an SDL window drawn with Vulkan.

pkg_check_modules(SDL3 IMPORTED_TARGET sdl3>=3.2)
pkg_check_modules(VULKAN IMPORTED_TARGET vulkan)
pkg_check_modules(PNG IMPORTED_TARGET libpng)
find_program(GLSLC glslc)
if(NOT SDL3_FOUND OR NOT VULKAN_FOUND OR NOT PNG_FOUND OR NOT GLSLC)
    message(FATAL_ERROR
        "opensu-retro needs SDL3, the Vulkan loader, libpng and glslc. Install them with "
        "`sudo dnf install SDL3-devel vulkan-loader-devel libpng-devel glslc` (Fedora) or "
        "`sudo apt install libsdl3-dev libvulkan-dev libpng-dev glslc` (Debian/Ubuntu)")
endif()

set(OPENSU_RETRO_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(OPENSU_RETRO_SHADERS)
foreach(stage vert frag)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/frame.${stage}")
    set(output "${OPENSU_RETRO_SHADER_DIR}/frame.${stage}.inc")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OPENSU_RETRO_SHADER_DIR}"
        COMMAND "${GLSLC}" -mfmt=num -o "${output}" "${source}"
        DEPENDS "${source}"
        COMMENT "Compiling frame.${stage} to SPIR-V")
    list(APPEND OPENSU_RETRO_SHADERS "${output}")
endforeach()

add_library(opensu_retro STATIC
    core.cpp core_environment.cpp core_options.cpp video_frame.cpp audio_sink.cpp
    audio_output.cpp joypad.cpp save_memory.cpp png_encoder.cpp vulkan_device.cpp presenter.cpp
    save_state.cpp control_server.cpp runner_app.cpp ${OPENSU_RETRO_SHADERS})
add_library(opensu::retro ALIAS opensu_retro)
target_include_directories(opensu_retro
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src"
    PRIVATE "${OPENSU_RETRO_SHADER_DIR}")
target_link_libraries(opensu_retro
    PUBLIC opensu::libretro_header opensu::pad opensu::fileio lucent::lucent PkgConfig::SDL3
           PkgConfig::VULKAN
    PRIVATE PkgConfig::PNG ${CMAKE_DL_LIBS})
target_compile_features(opensu_retro PUBLIC cxx_std_20)
target_compile_options(opensu_retro PRIVATE -Wall -Wextra -Wpedantic)

add_executable(opensu-retro main.cpp)
target_link_libraries(opensu-retro PRIVATE opensu::retro opensu::config lucent::lucent)
target_compile_options(opensu-retro PRIVATE -Wall -Wextra -Wpedantic)
# Beside opensu, in the build tree as installed, where the shell looks for it.
set_target_properties(opensu-retro PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src")
install(TARGETS opensu-retro RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
