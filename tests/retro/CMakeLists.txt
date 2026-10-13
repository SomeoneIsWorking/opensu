# The runner's owners through their shipping code, two fake cores, and opensu-retro end to end.
pkg_check_modules(TEST_PNG REQUIRED IMPORTED_TARGET libpng)

foreach(core fake_core fake_crashing_core)
    add_library(${core} SHARED fake_core.cpp)
    target_link_libraries(${core} PRIVATE opensu::libretro_header)
    target_compile_options(${core} PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(${core} PROPERTIES PREFIX "" CXX_VISIBILITY_PRESET hidden)
endforeach()
target_compile_definitions(fake_crashing_core PRIVATE FAKE_CORE_CRASH)

foreach(name video_frame audio_sink joypad save_memory core_environment core vulkan_messages)
    add_executable(retro_${name}_test ${name}_test.cpp)
    target_link_libraries(retro_${name}_test PRIVATE opensu::retro)
    target_include_directories(retro_${name}_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/../support")
    target_compile_definitions(retro_${name}_test
        PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
    target_compile_options(retro_${name}_test PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
foreach(name video_frame audio_sink joypad save_memory core_environment vulkan_messages)
    add_test(NAME retro_${name} COMMAND retro_${name}_test)
endforeach()
add_test(NAME retro_core COMMAND retro_core_test "$<TARGET_FILE:fake_core>")

add_executable(retro_runner_test runner_test.cpp)
target_link_libraries(retro_runner_test PRIVATE PkgConfig::TEST_PNG)
target_include_directories(retro_runner_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ui" "${CMAKE_CURRENT_SOURCE_DIR}/../support")
target_compile_definitions(retro_runner_test
    PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
target_compile_options(retro_runner_test PRIVATE -Wall -Wextra -Wpedantic)
add_dependencies(retro_runner_test opensu-retro fake_core fake_crashing_core)
add_test(NAME retro_runner
    COMMAND retro_runner_test "$<TARGET_FILE:opensu-retro>" "$<TARGET_FILE:fake_core>"
            "$<TARGET_FILE:fake_crashing_core>")
set_tests_properties(retro_runner PROPERTIES TIMEOUT 60)
