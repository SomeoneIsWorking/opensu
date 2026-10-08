# The home grid's model: layout, focus and motion. Pure, so tests link it alone.
add_library(iideck_grid STATIC home_layout.cpp grid_focus.cpp tile_motion.cpp tile_geometry.cpp rail_layout.cpp)
add_library(iideck::grid ALIAS iideck_grid)
target_include_directories(iideck_grid PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(iideck_grid PUBLIC cxx_std_20)
target_compile_options(iideck_grid PRIVATE -Wall -Wextra -Wpedantic)

# The chrome's models: top bar sizing, clock text, battery icon choice and the Guide menu. Pure, so tests link it alone.
add_library(iideck_hud_model STATIC top_bar_metrics.cpp clock_text.cpp battery_icon.cpp game_menu.cpp launch_panel.cpp
    section_view.cpp mode_chooser.cpp dock_metrics.cpp dock_motion.cpp icon_recolour.cpp)
add_library(iideck::hud_model ALIAS iideck_hud_model)
target_include_directories(iideck_hud_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_hud_model PUBLIC iideck::grid iideck::library)
target_compile_features(iideck_hud_model PUBLIC cxx_std_20)
target_compile_options(iideck_hud_model PRIVATE -Wall -Wextra -Wpedantic)

add_library(iideck_ui STATIC
    shell.cpp hud.cpp launcher_badges.cpp vector_icon.cpp status_pill.cpp progress_spinner.cpp glass.cpp tile_painter.cpp page_pill.cpp page_arrow.cpp
    button_glyph.cpp game_menu_painter.cpp launch_panel_painter.cpp dock_painter.cpp mode_chooser_painter.cpp rail_painter.cpp backdrop_blur.cpp
    round_shape.cpp
    typeface.cpp face_metrics.cpp platform.cpp platform_stroke.cpp glyph_textures.cpp)
add_library(iideck::ui ALIAS iideck_ui)
target_include_directories(iideck_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_ui
    PUBLIC iideck::grid iideck::hud_model iideck::library iideck::device
        iideck::config iideck::raylib
    PRIVATE lucent::lucent iideck::nanosvg
)
target_compile_features(iideck_ui PUBLIC cxx_std_20)
target_compile_options(iideck_ui PRIVATE -Wall -Wextra -Wpedantic)
