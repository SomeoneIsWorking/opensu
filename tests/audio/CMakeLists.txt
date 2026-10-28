add_executable(effect_test effect_test.cpp)
target_link_libraries(effect_test PRIVATE opensu::audio_model)
target_include_directories(effect_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(effect_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME effect COMMAND effect_test)

add_executable(volume_test volume_test.cpp)
target_link_libraries(volume_test PRIVATE opensu::audio_model)
target_include_directories(volume_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(volume_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME volume COMMAND volume_test)

pkg_check_modules(TEST_VORBISENC REQUIRED IMPORTED_TARGET vorbisenc vorbis ogg)
add_executable(sdl_sound_player_test sdl_sound_player_test.cpp)
target_link_libraries(sdl_sound_player_test
    PRIVATE opensu::audio PkgConfig::TEST_VORBISENC lucent::lucent)
target_include_directories(sdl_sound_player_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_definitions(sdl_sound_player_test
    PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
target_compile_options(sdl_sound_player_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sdl_sound_player COMMAND sdl_sound_player_test)

add_executable(shipped_sounds_test shipped_sounds_test.cpp)
target_link_libraries(shipped_sounds_test PRIVATE opensu::audio lucent::lucent)
target_include_directories(shipped_sounds_test PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_definitions(shipped_sounds_test PRIVATE OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
target_compile_options(shipped_sounds_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME shipped_sounds COMMAND shipped_sounds_test)
