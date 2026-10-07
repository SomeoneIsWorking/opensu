add_library(iideck_gamepad STATIC reader.cpp)
add_library(iideck::gamepad ALIAS iideck_gamepad)
target_include_directories(iideck_gamepad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_gamepad
    PUBLIC iideck::raylib
    PRIVATE lucent::lucent
)
target_compile_features(iideck_gamepad PUBLIC cxx_std_20)
