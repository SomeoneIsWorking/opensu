# The home grid's model: layout, focus and motion. Pure, so tests link it alone.
add_library(opensu_grid STATIC home_layout.cpp grid_focus.cpp tile_motion.cpp tile_geometry.cpp rail_layout.cpp icon_size.cpp)
add_library(opensu::grid ALIAS opensu_grid)
target_include_directories(opensu_grid PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(opensu_grid PUBLIC cxx_std_20)
target_compile_options(opensu_grid PRIVATE -Wall -Wextra -Wpedantic)

# The chrome's models: top bar sizing, clock text, battery icon choice and the Guide menus. Pure, so tests link it alone.
add_library(opensu_hud_model STATIC top_bar_metrics.cpp top_bar_layout.cpp corner_hints.cpp clock_text.cpp battery_icon.cpp guide_menu.cpp quick_menu.cpp launch_panel.cpp
    section_view.cpp mode_chooser.cpp dock_metrics.cpp dock_motion.cpp icon_recolour.cpp
    search_panel.cpp context_menu.cpp panel_fade.cpp breadcrumbs.cpp details_page.cpp
    settings_page.cpp folder_chooser.cpp volume_osd.cpp)
add_library(opensu::hud_model ALIAS opensu_hud_model)
target_include_directories(opensu_hud_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_hud_model PUBLIC opensu::grid opensu::library)
target_compile_features(opensu_hud_model PUBLIC cxx_std_20)
target_compile_options(opensu_hud_model PRIVATE -Wall -Wextra -Wpedantic)

add_library(opensu_ui STATIC
    shell.cpp hud.cpp breadcrumb_painter.cpp details_page_painter.cpp launcher_badges.cpp vector_icon.cpp status_pill.cpp progress_spinner.cpp glass.cpp tile_painter.cpp page_pill.cpp page_arrow.cpp
    button_glyph.cpp guide_menu_painter.cpp quick_menu_painter.cpp guide_panels.cpp page_panel.cpp tile_artwork.cpp launch_panel_painter.cpp dock_painter.cpp mode_chooser_painter.cpp search_panel_painter.cpp context_menu_painter.cpp rail_painter.cpp backdrop_blur.cpp
    round_shape.cpp image_decoder.cpp clip_stack.cpp row_controls.cpp
    settings_page_painter.cpp folder_chooser_painter.cpp settings_panels.cpp volume_osd_painter.cpp
    typeface.cpp face_metrics.cpp platform.cpp platform_stroke.cpp glyph_textures.cpp)
add_library(opensu::ui ALIAS opensu_ui)
target_include_directories(opensu_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_ui
    PUBLIC opensu::grid opensu::hud_model opensu::library opensu::device opensu::input
        opensu::config opensu::raylib
    PRIVATE lucent::lucent opensu::nanosvg
)
target_compile_features(opensu_ui PUBLIC cxx_std_20)
target_compile_options(opensu_ui PRIVATE -Wall -Wextra -Wpedantic)
