add_library(iideck_library STATIC
    game.cpp
    catalog.cpp
    steam.cpp
    epic.cpp
    gog.cpp
    roms.cpp
    rom_systems.cpp
    shelf.cpp
    emulators.cpp
)
add_library(iideck::library ALIAS iideck_library)
target_include_directories(iideck_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_library
    PUBLIC iideck::vdf
        iideck::config
    PRIVATE lucent::lucent
)
target_compile_features(iideck_library PUBLIC cxx_std_20)