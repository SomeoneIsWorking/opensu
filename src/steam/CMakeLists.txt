add_library(iideck_steam STATIC client.cpp desktop_steam.cpp devtools.cpp downloads.cpp
    install_wizard.cpp)
add_library(iideck::steam ALIAS iideck_steam)
target_include_directories(iideck_steam PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_steam PUBLIC iideck::launch iideck::library lucent::lucent Threads::Threads)
target_compile_features(iideck_steam PUBLIC cxx_std_20)
