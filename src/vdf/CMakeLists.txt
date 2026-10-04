add_library(iideck_vdf STATIC node.cpp parser.cpp)
add_library(iideck::vdf ALIAS iideck_vdf)
target_include_directories(iideck_vdf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(iideck_vdf PUBLIC cxx_std_20)