# The home grid's model: layout, focus and motion. Pure, so tests link it alone.
add_library(iideck_grid STATIC home_layout.cpp grid_focus.cpp tile_motion.cpp tile_geometry.cpp)
add_library(iideck::grid ALIAS iideck_grid)
target_include_directories(iideck_grid PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(iideck_grid PUBLIC cxx_std_20)
target_compile_options(iideck_grid PRIVATE -Wall -Wextra -Wpedantic)

add_library(iideck_ui STATIC
    shell.cpp hud.cpp tile_painter.cpp page_pill.cpp round_shape.cpp
    typeface.cpp platform.cpp platform_stroke.cpp)
add_library(iideck::ui ALIAS iideck_ui)
target_include_directories(iideck_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_ui
    PUBLIC iideck::grid iideck::library
        iideck::config raylib
    PRIVATE lucent::lucent
)
target_include_directories(iideck_ui PUBLIC "${RAYLIB_ROOT}/include")
target_link_directories(iideck_ui PUBLIC "${RAYLIB_ROOT}/lib")
target_compile_features(iideck_ui PUBLIC cxx_std_20)
target_compile_options(iideck_ui PRIVATE -Wall -Wextra -Wpedantic)
