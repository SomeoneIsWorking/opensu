# Interface themes as folders of data (docs/design/interface-themes.md) and the folder rule every
# kind of theme follows: pure, so tests link it alone.
add_library(opensu_theme STATIC theme_folders.cpp interface_theme.cpp interface_themes.cpp)
add_library(opensu::theme ALIAS opensu_theme)
target_include_directories(opensu_theme PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_theme PUBLIC opensu::settings
    PRIVATE nlohmann_json::nlohmann_json lucent::lucent)
target_compile_features(opensu_theme PUBLIC cxx_std_20)
opensu_warnings(opensu_theme)
