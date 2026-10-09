# The UI sounds' model: which files and the repeat rule. Pure, so tests link it alone.
add_library(opensu_audio_model STATIC effect.cpp debounce.cpp volume_backend.cpp system_volume.cpp)
add_library(opensu::audio_model ALIAS opensu_audio_model)
target_include_directories(opensu_audio_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(opensu_audio_model PUBLIC cxx_std_20)
target_compile_options(opensu_audio_model PRIVATE -Wall -Wextra -Wpedantic)

add_library(opensu_audio STATIC sound_player.cpp)
add_library(opensu::audio ALIAS opensu_audio)
target_include_directories(opensu_audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_audio PUBLIC opensu::audio_model opensu::raylib
                                   PRIVATE lucent::lucent)
target_compile_features(opensu_audio PUBLIC cxx_std_20)
target_compile_options(opensu_audio PRIVATE -Wall -Wextra -Wpedantic)
