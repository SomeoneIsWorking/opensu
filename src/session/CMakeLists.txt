add_library(iideck_session STATIC gamescope.cpp monitor.cpp nested_session.cpp)
add_library(iideck::session ALIAS iideck_session)
target_include_directories(iideck_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(iideck_session
    PUBLIC iideck::launch lucent::lucent
    PRIVATE iideck::raylib
)
target_compile_features(iideck_session PUBLIC cxx_std_20)
