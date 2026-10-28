# vcpkg's x64-windows built by clang-cl through openSU's toolchain, on Windows and on a Linux host.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../../cmake/toolchains/windows-clang-cl.cmake")
# Release only: the openSU build links the release CRT, which is all an xwin SDK carries.
set(VCPKG_BUILD_TYPE release)
# A chainloaded toolchain turns vcvars off; the ports still need the SDK's tools and nmake (OpenSSL).
if(CMAKE_HOST_WIN32)
    set(VCPKG_LOAD_VCVARS_ENV ON)
endif()
# vcpkg takes x86_64 Linux to x86_64 Windows for a native meson build; its own machine file, which
# names the Windows host, is handed back to meson as the cross file.
if(NOT CMAKE_HOST_WIN32)
    set(VCPKG_MESON_CROSS_FILE_RELEASE "${CURRENT_BUILDTREES_DIR}/meson-${TARGET_TRIPLET}-rel.log")
endif()
