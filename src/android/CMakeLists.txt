# Calls from native code into the process's SDL Activity (the shell's, or the runner's in `:retro`)
# over JNI; Android's alone.
add_library(opensu_android STATIC activity_call.cpp)
add_library(opensu::android ALIAS opensu_android)
target_include_directories(opensu_android PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_android PUBLIC PkgConfig::SDL3)
target_compile_features(opensu_android PUBLIC cxx_std_20)
opensu_warnings(opensu_android)
