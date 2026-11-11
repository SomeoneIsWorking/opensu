# libzt, the embedded ZeroTier node Lucent's `lucent::zerotier` links, at the fork's pinned commit
# with its submodules (ZeroTierOne from the fork, lwIP). Static, the C API only on Android.
# Apache-2.0 since each pinned file's BSL Change Date (docs/prior-art.md).

set(root "${CMAKE_BINARY_DIR}/libzt")
set(stage "${root}/prefix")

set(forwarded "")
foreach(variable IN ITEMS CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_TOOLCHAIN_FILE ANDROID_ABI
        ANDROID_PLATFORM ANDROID_STL CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET)
    if(DEFINED ${variable})
        list(APPEND forwarded "-D${variable}=${${variable}}")
    endif()
endforeach()

ExternalProject_Add(opensu_libzt
    GIT_REPOSITORY "${OPENSU_LIBZT_REPOSITORY}"
    GIT_TAG "${OPENSU_LIBZT_COMMIT}"
    GIT_SUBMODULES_RECURSE ON
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/source"
    BINARY_DIR "${root}/build"
    INSTALL_DIR "${stage}"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        ${forwarded}
        "-DCMAKE_INSTALL_PREFIX=${stage}"
        "-DBUILD_SHARED_LIB=OFF"
        "-DBUILD_HOST_SELFTEST=OFF"
        "-DZTS_NDK_ONLY=ON"
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuLibztConfig.cmake" @ONLY CONTENT [=[
# libzt from openSU's shared third-party prefix, at the commit openSU pins; the prefix also holds
# libzt's own package (`find_package(libzt)`, `libzt::zt`), which Lucent finds.
set(OpensuLibzt_COMMIT "@OPENSU_LIBZT_COMMIT@")
]=])
install(DIRECTORY "${stage}/" DESTINATION .)
install(FILES "${root}/OpensuLibztConfig.cmake" DESTINATION lib/cmake/OpensuLibzt)
