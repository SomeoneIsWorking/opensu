# The UI sounds' model: which files and the repeat rule. Pure, so tests link it alone.
add_library(iideck_audio_model STATIC effect.cpp debounce.cpp)
add_library(iideck::audio_model ALIAS iideck_audio_model)
target_include_directories(iideck_audio_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(iideck_audio_model PUBLIC cxx_std_20)
target_compile_options(iideck_audio_model PRIVATE -Wall -Wextra -Wpedantic)

add_library(iideck_audio STATIC sound_player.cpp)
add_library(iideck::audio ALIAS iideck_audio)
target_include_directories(iideck_audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_audio PUBLIC iideck::audio_model iideck::raylib
                                   PRIVATE lucent::lucent)
target_compile_features(iideck_audio PUBLIC cxx_std_20)
target_compile_options(iideck_audio PRIVATE -Wall -Wextra -Wpedantic)
