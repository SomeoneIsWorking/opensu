add_library(opensu_vdf STATIC node.cpp parser.cpp)
add_library(opensu::vdf ALIAS opensu_vdf)
target_include_directories(opensu_vdf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(opensu_vdf PUBLIC cxx_std_20)