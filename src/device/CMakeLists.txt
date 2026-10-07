# Host device state the shell shows: the system battery.
add_library(iideck_device STATIC battery.cpp)
add_library(iideck::device ALIAS iideck_device)
target_include_directories(iideck_device PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(iideck_device PUBLIC cxx_std_20)
target_compile_options(iideck_device PRIVATE -Wall -Wextra -Wpedantic)
