# Opus, remote play's sound, for Android, whose prefix has none: the desktop's comes from the system
# or vcpkg. Static, at the release pinned. BSD-3-Clause.

set(root "${CMAKE_BINARY_DIR}/opus")
set(stage "${root}/prefix")

set(forwarded "")
foreach(variable IN ITEMS CMAKE_TOOLCHAIN_FILE ANDROID_ABI ANDROID_PLATFORM ANDROID_STL)
    if(DEFINED ${variable})
        list(APPEND forwarded "-D${variable}=${${variable}}")
    endif()
endforeach()

ExternalProject_Add(opensu_opus
    GIT_REPOSITORY "${OPENSU_OPUS_REPOSITORY}"
    GIT_TAG "${OPENSU_OPUS_COMMIT}"
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/source"
    BINARY_DIR "${root}/build"
    INSTALL_DIR "${stage}"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        ${forwarded}
        "-DCMAKE_INSTALL_PREFIX=${stage}"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DOPUS_BUILD_PROGRAMS=OFF"
        "-DOPUS_BUILD_TESTING=OFF"
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuOpusConfig.cmake" @ONLY CONTENT [=[
# Opus from openSU's shared third-party prefix, at the commit openSU pins.
get_filename_component(_opensu_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(OpensuOpus_COMMIT "@OPENSU_OPUS_COMMIT@")
set(OpensuOpus_LIBRARY "${_opensu_prefix}/lib/libopus.a")
set(OpensuOpus_INCLUDE_DIR "${_opensu_prefix}/include/opus")
unset(_opensu_prefix)
]=])
install(FILES "${stage}/lib/libopus.a" DESTINATION lib)
install(DIRECTORY "${stage}/include/opus" DESTINATION include)
install(FILES "${root}/OpensuOpusConfig.cmake" DESTINATION lib/cmake/OpensuOpus)
