add_library(iideck_app STATIC shell_app.cpp)
add_library(iideck::app ALIAS iideck_app)
target_include_directories(iideck_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_app
    PUBLIC
        iideck::library
        iideck::ui
        iideck::gamepad
        iideck::launch
        raylib
        lucent::lucent
)
target_include_directories(iideck_app PUBLIC "${RAYLIB_ROOT}/include")
target_link_directories(iideck_app PUBLIC "${RAYLIB_ROOT}/lib")
target_compile_features(iideck_app PUBLIC cxx_std_20)