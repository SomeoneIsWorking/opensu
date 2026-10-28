add_library(opensu_config STATIC config.cpp arguments.cpp)
add_library(opensu::config ALIAS opensu_config)
# The user directories and openSU's own files: Lucent's resolvers and the executable's folder on the
# desktop; the Activity's storage and the APK's native libraries on Android.
if(ANDROID)
    target_sources(opensu_config PRIVATE locations_android.cpp install_layout_android.cpp)
else()
    target_sources(opensu_config PRIVATE locations.cpp install_layout_beside.cpp)
endif()
target_include_directories(opensu_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_config PUBLIC opensu::command lucent::lucent PRIVATE opensu::clock PkgConfig::SDL3)
target_compile_features(opensu_config PUBLIC cxx_std_20)
target_compile_definitions(opensu_config PUBLIC OPENSU_GAMESCOPE_RELATIVE="${OPENSU_GAMESCOPE_RELATIVE}"
    OPENSU_LIBRASHADER_RELATIVE="${OPENSU_LIBRASHADER_RELATIVE}"
    OPENSU_EXECUTABLE_SUFFIX="${CMAKE_EXECUTABLE_SUFFIX}")
# The folder each OS's directories name: Linux spells its XDG folders in lower case.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(opensu_config PRIVATE OPENSU_LOCATION_NAME="opensu")
else()
    target_compile_definitions(opensu_config PRIVATE OPENSU_LOCATION_NAME="openSU")
endif()
