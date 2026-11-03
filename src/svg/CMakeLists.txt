# SVG files rasterised through nanosvg, for the shell's icons and the runner's touch controls.
add_library(opensu_svg STATIC svg_raster.cpp)
add_library(opensu::svg ALIAS opensu_svg)
target_include_directories(opensu_svg PUBLIC "${CMAKE_SOURCE_DIR}/src")
# gpu2d::image carries gpu2d/geometry.hpp's colour.
target_link_libraries(opensu_svg PUBLIC gpu2d::image PRIVATE opensu::nanosvg)
target_compile_features(opensu_svg PUBLIC cxx_std_20)
opensu_warnings(opensu_svg)

# The macOS package's app icon, rendered from packaging/opensu.svg by tools/package/macos.
if(APPLE)
    add_executable(opensu-svg-png svg_png.cpp)
    target_link_libraries(opensu-svg-png PRIVATE opensu::svg lucent::lucent)
    opensu_warnings(opensu-svg-png)
endif()
