add_library(opensu_config STATIC config.cpp locations.cpp arguments.cpp)
add_library(opensu::config ALIAS opensu_config)
target_include_directories(opensu_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_config PUBLIC opensu::command lucent::lucent)
target_compile_features(opensu_config PUBLIC cxx_std_20)
target_compile_definitions(opensu_config PUBLIC OPENSU_GAMESCOPE_RELATIVE="${OPENSU_GAMESCOPE_RELATIVE}"
    OPENSU_LIBRASHADER_RELATIVE="${OPENSU_LIBRASHADER_RELATIVE}")
# The folder each OS's directories name: Linux spells its XDG folders in lower case.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(opensu_config PRIVATE OPENSU_LOCATION_NAME="opensu")
else()
    target_compile_definitions(opensu_config PRIVATE OPENSU_LOCATION_NAME="openSU")
endif()
