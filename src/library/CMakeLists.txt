add_library(iideck_library STATIC
    game.cpp
    catalog.cpp
    steam.cpp
    epic.cpp
    gog.cpp
    roms.cpp
)
add_library(iideck::library ALIAS iideck_library)
target_include_directories(iideck_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_library
    PUBLIC iideck::vdf
    PRIVATE lucent::lucent
)
target_compile_features(iideck_library PUBLIC cxx_std_20)