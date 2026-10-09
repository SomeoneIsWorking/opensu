foreach(name home_layout grid_focus tile_motion tile_geometry rail_layout)
    add_executable(${name}_test ${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE opensu::grid)
    target_compile_options(${name}_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()

foreach(name top_bar_metrics top_bar_layout corner_hints clock_text game_menu launch_panel dock_metrics dock_motion
             section_view mode_chooser icon_recolour search_panel context_menu panel_fade breadcrumbs details_page settings_page volume_osd)
    add_executable(${name}_test ${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE opensu::hud_model)
    target_compile_options(${name}_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()

add_executable(clip_stack_test clip_stack_test.cpp)
target_link_libraries(clip_stack_test PRIVATE opensu::ui)
target_compile_options(clip_stack_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME clip_stack COMMAND clip_stack_test)

add_executable(round_shape_test round_shape_test.cpp)
target_link_libraries(round_shape_test PRIVATE opensu::ui)
target_compile_options(round_shape_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME round_shape COMMAND round_shape_test)

add_executable(face_metrics_test face_metrics_test.cpp)
target_link_libraries(face_metrics_test PRIVATE opensu::ui)
target_compile_options(face_metrics_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME face_metrics
    COMMAND face_metrics_test "${CMAKE_SOURCE_DIR}/assets/CalSans-Regular.ttf")

add_executable(platform_test platform_test.cpp)
target_link_libraries(platform_test PRIVATE opensu::ui)
target_compile_options(platform_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME platform COMMAND platform_test)

add_executable(progress_spinner_test progress_spinner_test.cpp)
target_link_libraries(progress_spinner_test PRIVATE opensu::ui)
target_compile_options(progress_spinner_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME progress_spinner COMMAND progress_spinner_test)

add_executable(vector_icon_test vector_icon_test.cpp)
target_link_libraries(vector_icon_test PRIVATE opensu::ui)
target_compile_options(vector_icon_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME vector_icon COMMAND vector_icon_test "${CMAKE_SOURCE_DIR}/assets")

add_executable(image_decoder_test image_decoder_test.cpp)
target_link_libraries(image_decoder_test PRIVATE opensu::ui)
target_compile_definitions(image_decoder_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
target_compile_options(image_decoder_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME image_decoder COMMAND image_decoder_test)

add_executable(folder_chooser_test folder_chooser_test.cpp)
target_link_libraries(folder_chooser_test PRIVATE opensu::hud_model)
target_compile_options(folder_chooser_test PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(folder_chooser_test PRIVATE OPENSU_TEST_SCRATCH="${CMAKE_BINARY_DIR}/test-scratch")
add_test(NAME folder_chooser COMMAND folder_chooser_test)
