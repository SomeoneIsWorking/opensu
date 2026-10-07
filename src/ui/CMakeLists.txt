# The home grid's model: layout, focus and motion. Pure, so tests link it alone.
add_library(iideck_grid STATIC home_layout.cpp grid_focus.cpp tile_motion.cpp tile_geometry.cpp)
add_library(iideck::grid ALIAS iideck_grid)
target_include_directories(iideck_grid PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(iideck_grid PUBLIC cxx_std_20)
target_compile_options(iideck_grid PRIVATE -Wall -Wextra -Wpedantic)

# The top bar's model: sizing, clock text and battery icon choice. Pure, so tests link it alone.
add_library(iideck_hud_model STATIC top_bar_metrics.cpp clock_text.cpp battery_icon.cpp)
add_library(iideck::hud_model ALIAS iideck_hud_model)
target_include_directories(iideck_hud_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(iideck_hud_model PUBLIC cxx_std_20)
target_compile_options(iideck_hud_model PRIVATE -Wall -Wextra -Wpedantic)

add_library(iideck_ui STATIC
    shell.cpp hud.cpp status_pill.cpp glass.cpp tile_painter.cpp page_pill.cpp page_arrow.cpp
    round_shape.cpp
    typeface.cpp platform.cpp platform_stroke.cpp)
add_library(iideck::ui ALIAS iideck_ui)
target_include_directories(iideck_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_ui
    PUBLIC iideck::grid iideck::hud_model iideck::library iideck::device
        iideck::config iideck::raylib
    PRIVATE lucent::lucent
)
target_compile_features(iideck_ui PUBLIC cxx_std_20)
target_compile_options(iideck_ui PRIVATE -Wall -Wextra -Wpedantic)
