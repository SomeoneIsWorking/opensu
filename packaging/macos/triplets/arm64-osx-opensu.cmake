# openSU's macOS ports: dylibs, which the app carries in Contents/Frameworks (pkg-config's flags for
# a static SDL3 leave out its frameworks), release only, for the app's oldest macOS.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET 14.0)
set(VCPKG_BUILD_TYPE release)
