# The home grid's model: layout, focus and motion. Pure, so tests link it alone.
add_library(opensu_grid STATIC home_layout.cpp grid_focus.cpp spatial_focus.cpp tile_motion.cpp tile_geometry.cpp rail_layout.cpp icon_size.cpp)
add_library(opensu::grid ALIAS opensu_grid)
target_include_directories(opensu_grid PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# The toolkit's focus directions.
target_link_libraries(opensu_grid PUBLIC gpu2d::ui)
target_compile_features(opensu_grid PUBLIC cxx_std_20)
target_compile_options(opensu_grid PRIVATE -Wall -Wextra -Wpedantic)

# The chrome's models: top bar sizing, clock text, battery icon choice, the chord list and the mapping editor. Pure, so tests link it alone.
add_library(opensu_hud_model STATIC top_bar_metrics.cpp top_bar_layout.cpp corner_hints.cpp clock_text.cpp battery_icon.cpp
    section_view.cpp dock_metrics.cpp dock_motion.cpp
    on_screen_keyboard.cpp panel_fade.cpp breadcrumbs.cpp game_details.cpp
    setting_row.cpp folder_chooser.cpp volume_osd.cpp chord_hint.cpp
    mapping_board_layout.cpp mapping_page.cpp)
add_library(opensu::hud_model ALIAS opensu_hud_model)
target_include_directories(opensu_hud_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_hud_model PUBLIC opensu::grid opensu::library opensu::input)
target_compile_features(opensu_hud_model PUBLIC cxx_std_20)
target_compile_options(opensu_hud_model PRIVATE -Wall -Wextra -Wpedantic)

add_library(opensu_ui STATIC
    shell.cpp home_grid.cpp home_dock.cpp chrome_panel.cpp top_bar_chrome.cpp keyboard_chrome.cpp mapping_chrome.cpp hud.cpp breadcrumb_painter.cpp launcher_badges.cpp vector_icon.cpp status_pill.cpp progress_spinner.cpp glass.cpp tile_painter.cpp console_art.cpp page_pill.cpp page_arrow.cpp
    button_glyph.cpp tile_artwork.cpp dock_painter.cpp on_screen_keyboard_painter.cpp rail_painter.cpp
    round_shape.cpp image_decoder.cpp
    volume_osd_painter.cpp chord_hint_painter.cpp
    mapping_page_painter.cpp hint_row.cpp
    interface_input.cpp interface_theme.cpp interface_text.cpp screen_stack.cpp row_widgets.cpp page_frame.cpp screen_images.cpp image_view.cpp layout_sketch.cpp progress_screen.cpp entry_screen.cpp offer_card.cpp
    platform.cpp)
add_library(opensu::ui ALIAS opensu_ui)
target_include_directories(opensu_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_ui
    PUBLIC opensu::grid opensu::hud_model opensu::library opensu::device opensu::input
        opensu::config gpu2d::render gpu2d::ui
    PRIVATE lucent::lucent opensu::nanosvg
)
target_compile_features(opensu_ui PUBLIC cxx_std_20)
target_compile_options(opensu_ui PRIVATE -Wall -Wextra -Wpedantic)
