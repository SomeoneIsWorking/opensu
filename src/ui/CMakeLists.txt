add_library(iideck_ui STATIC shell.cpp typeface.cpp)
add_library(iideck::ui ALIAS iideck_ui)
target_include_directories(iideck_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_ui
    PUBLIC iideck::library
        iideck::config raylib
    PRIVATE lucent::lucent
)
target_include_directories(iideck_ui PUBLIC "${RAYLIB_ROOT}/include")
target_link_directories(iideck_ui PUBLIC "${RAYLIB_ROOT}/lib")
target_compile_features(iideck_ui PUBLIC cxx_std_20)