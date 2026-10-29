add_executable(effect_test effect_test.cpp)
target_link_libraries(effect_test PRIVATE opensu::audio_model)
opensu_warnings(effect_test)
add_test(NAME effect COMMAND effect_test)

add_executable(volume_test volume_test.cpp)
target_link_libraries(volume_test PRIVATE opensu::audio_model)
opensu_warnings(volume_test)
add_test(NAME volume COMMAND volume_test)

pkg_check_modules(TEST_VORBISENC REQUIRED IMPORTED_TARGET vorbisenc vorbis ogg)
add_executable(sdl_sound_player_test sdl_sound_player_test.cpp)
target_link_libraries(sdl_sound_player_test
    PRIVATE opensu::audio opensu::command PkgConfig::TEST_VORBISENC lucent::lucent)
target_compile_definitions(sdl_sound_player_test
    PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
opensu_warnings(sdl_sound_player_test)
add_test(NAME sdl_sound_player COMMAND sdl_sound_player_test)

add_executable(shipped_sounds_test shipped_sounds_test.cpp)
target_link_libraries(shipped_sounds_test PRIVATE opensu::audio lucent::lucent)
target_compile_definitions(shipped_sounds_test PRIVATE OPENSU_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
opensu_warnings(shipped_sounds_test)
add_test(NAME shipped_sounds COMMAND shipped_sounds_test)
