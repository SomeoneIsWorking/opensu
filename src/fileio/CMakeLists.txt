add_library(opensu_fileio STATIC atomic_write.cpp)
add_library(opensu::fileio ALIAS opensu_fileio)
if(WIN32)
    target_sources(opensu_fileio PRIVATE instance_lock_win32.cpp owner_only_write_win32.cpp
        folder_access_win32.cpp)
else()
    target_sources(opensu_fileio PRIVATE instance_lock_posix.cpp owner_only_write_posix.cpp
        folder_access_posix.cpp)
endif()
target_include_directories(opensu_fileio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(opensu_fileio PRIVATE lucent::lucent)
target_compile_features(opensu_fileio PUBLIC cxx_std_20)
opensu_warnings(opensu_fileio)
