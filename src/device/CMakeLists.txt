# Host device state the shell shows: the system battery, the controllers' batteries and the real
# Wii Remotes, read from the kernel's HID devices on Linux.
add_library(opensu_device STATIC battery.cpp controller_batteries_sysfs.cpp)
add_library(opensu::device ALIAS opensu_device)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(opensu_device PRIVATE wii_remotes_sysfs.cpp)
else()
    target_sources(opensu_device PRIVATE wii_remotes_none.cpp)
endif()
target_include_directories(opensu_device PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(opensu_device PUBLIC PkgConfig::SDL3)
target_compile_features(opensu_device PUBLIC cxx_std_20)
opensu_warnings(opensu_device)
