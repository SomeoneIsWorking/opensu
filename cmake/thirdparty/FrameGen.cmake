# framegen (MIT), the frame pacer and Lossless generator, from the Gamescope fork's `framegen/` at the
# commit openSU pins for Gamescope, so the guest and the host's Gamescope run one revision of it.
# Only the SPIR-V headers submodule is fetched; a static library with its headers, built with this
# prefix's compilers or the NDK's ABI, API level and C++ runtime.

set(root "${CMAKE_BINARY_DIR}/framegen")
set(stage "${root}/prefix")

set(forwarded "")
foreach(variable IN ITEMS CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_TOOLCHAIN_FILE ANDROID_ABI
        ANDROID_PLATFORM ANDROID_STL)
    if(DEFINED ${variable})
        list(APPEND forwarded "-D${variable}=${${variable}}")
    endif()
endforeach()

ExternalProject_Add(opensu_framegen
    GIT_REPOSITORY "${OPENSU_GAMESCOPE_REPOSITORY}"
    GIT_TAG "${OPENSU_GAMESCOPE_COMMIT}"
    GIT_SUBMODULES "thirdparty/SPIRV-Headers"
    GIT_PROGRESS ON
    SOURCE_DIR "${root}/source"
    SOURCE_SUBDIR framegen
    BINARY_DIR "${root}/build"
    INSTALL_DIR "${stage}"
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE=Release"
        ${forwarded}
        "-DCMAKE_INSTALL_PREFIX=${stage}"
        "-DCMAKE_INSTALL_LIBDIR=lib"
    USES_TERMINAL_DOWNLOAD ON
    USES_TERMINAL_BUILD ON
)

file(CONFIGURE OUTPUT "${root}/OpensuFrameGenConfig.cmake" @ONLY CONTENT [=[
# framegen from openSU's shared third-party prefix, at the Gamescope commit openSU pins.
set(OpensuFrameGen_COMMIT "@OPENSU_GAMESCOPE_COMMIT@")
include("${CMAKE_CURRENT_LIST_DIR}/../framegen/framegenConfig.cmake")
]=])
install(DIRECTORY "${stage}/lib/" DESTINATION lib)
install(DIRECTORY "${stage}/include/framegen" DESTINATION include)
install(DIRECTORY "${stage}/share/licenses/framegen" DESTINATION share/licenses)
install(FILES "${root}/OpensuFrameGenConfig.cmake" DESTINATION lib/cmake/OpensuFrameGen)
