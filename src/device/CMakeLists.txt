# Host device state the shell shows: the system battery.
add_library(opensu_device STATIC battery.cpp)
add_library(opensu::device ALIAS opensu_device)
target_include_directories(opensu_device PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_compile_features(opensu_device PUBLIC cxx_std_20)
target_compile_options(opensu_device PRIVATE -Wall -Wextra -Wpedantic)
