add_library(opensu_command STATIC command.cpp)
add_library(opensu::command ALIAS opensu_command)
# A short-lived child: posix_spawn in a process group, or CreateProcessW in a Job Object. Its pipes
# close on exec through pipe2, which macOS lacks.
if(WIN32)
    target_sources(opensu_command PRIVATE win32_child.cpp win32.cpp)
else()
    target_sources(opensu_command PRIVATE posix_child.cpp child_environment.cpp)
    if(APPLE)
        target_sources(opensu_command PRIVATE child_pipe_darwin.cpp)
    else()
        target_sources(opensu_command PRIVATE child_pipe_linux.cpp)
    endif()
endif()
target_include_directories(opensu_command PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_command PUBLIC lucent::lucent)
target_compile_features(opensu_command PUBLIC cxx_std_20)

add_library(opensu_launch STATIC handoff.cpp launch_progress.cpp process_tree.cpp)
add_library(opensu::launch ALIAS opensu_launch)
# The process table: /proc on Linux, Toolhelp32 on Windows, libproc and sysctl on macOS; Android
# has none, so no game is found by its hint there.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(opensu_launch PRIVATE process_table_proc.cpp)
elseif(WIN32)
    target_sources(opensu_launch PRIVATE process_table_toolhelp.cpp)
    target_link_libraries(opensu_launch PRIVATE shell32)
elseif(APPLE)
    target_sources(opensu_launch PRIVATE process_table_darwin.cpp)
else()
    target_sources(opensu_launch PRIVATE process_table_none.cpp)
endif()
# How a Steam game is handed to the client: `steam -applaunch`, or the steam:// link through the
# Windows shell or macOS's `open`.
if(WIN32)
    target_sources(opensu_launch PRIVATE steam_request_url.cpp)
elseif(APPLE)
    target_sources(opensu_launch PRIVATE steam_request_open.cpp)
else()
    target_sources(opensu_launch PRIVATE steam_request_applaunch.cpp)
endif()
# The launcher's backend: a systemd user scope on Linux, a Job Object on Windows and a process
# group on macOS (the desktop's windows for the handoff on both), an intent from the shell's
# Activity on Android.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(opensu_launch PRIVATE process_launcher_systemd_scope.cpp)
elseif(WIN32)
    target_sources(opensu_launch PRIVATE process_launcher_job_object.cpp win32_windows.cpp)
elseif(ANDROID)
    target_sources(opensu_launch PRIVATE process_launcher_intent.cpp)
    target_link_libraries(opensu_launch PRIVATE opensu::android)
elseif(APPLE)
    target_sources(opensu_launch PRIVATE process_launcher_process_group.cpp quartz_windows.cpp)
    target_link_libraries(opensu_launch PRIVATE "-framework CoreGraphics"
        "-framework CoreFoundation")
endif()
# What a launch can start: an executable file, except on Android, where the intent decides.
if(NOT ANDROID)
    target_sources(opensu_launch PRIVATE process_launcher_executable.cpp)
endif()
target_include_directories(opensu_launch PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(opensu_launch PUBLIC opensu::command opensu::library lucent::lucent)
target_compile_features(opensu_launch PUBLIC cxx_std_20)
