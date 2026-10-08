foreach(name home_layout grid_focus tile_motion tile_geometry)
    add_executable(${name}_test ${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE iideck::grid)
    target_compile_options(${name}_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()

foreach(name top_bar_metrics clock_text game_menu)
    add_executable(${name}_test ${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE iideck::hud_model)
    target_compile_options(${name}_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name}_test)
endforeach()

add_executable(round_shape_test round_shape_test.cpp)
target_link_libraries(round_shape_test PRIVATE iideck::ui)
target_compile_options(round_shape_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME round_shape COMMAND round_shape_test)

add_executable(face_metrics_test face_metrics_test.cpp)
target_link_libraries(face_metrics_test PRIVATE iideck::ui)
target_compile_options(face_metrics_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME face_metrics
    COMMAND face_metrics_test "${CMAKE_SOURCE_DIR}/assets/CalSans-Regular.ttf")

add_executable(platform_test platform_test.cpp)
target_link_libraries(platform_test PRIVATE iideck::ui)
target_compile_options(platform_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME platform COMMAND platform_test)
