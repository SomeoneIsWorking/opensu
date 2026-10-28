# The UI sounds' model: which files and the repeat rule. Pure, so tests link it alone.
add_library(opensu_audio_model STATIC effect.cpp debounce.cpp volume_backend.cpp system_volume.cpp)
add_library(opensu::audio_model ALIAS opensu_audio_model)
target_link_libraries(opensu_audio_model PRIVATE nlohmann_json::nlohmann_json lucent::lucent)
target_include_directories(opensu_audio_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(opensu_audio_model PUBLIC cxx_std_20)
target_compile_options(opensu_audio_model PRIVATE -Wall -Wextra -Wpedantic)

# The UI sounds on SDL audio streams; WAV through SDL, OGG through libvorbisfile.
add_library(opensu_audio STATIC sound_clip.cpp sdl_sound_player.cpp)
add_library(opensu::audio ALIAS opensu_audio)
target_include_directories(opensu_audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_audio
    PUBLIC opensu::audio_model PkgConfig::SDL3
    PRIVATE PkgConfig::VORBISFILE lucent::lucent)
target_compile_features(opensu_audio PUBLIC cxx_std_20)
target_compile_options(opensu_audio PRIVATE -Wall -Wextra -Wpedantic)
if(ANDROID)
    target_sources(opensu_audio PRIVATE audio_manager_volume.cpp)
    target_link_libraries(opensu_audio PRIVATE opensu::android)
endif()
