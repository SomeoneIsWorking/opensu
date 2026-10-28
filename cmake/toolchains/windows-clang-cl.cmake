# Windows x64 with clang-cl, the MSVC STL and the Windows SDK. On Windows the SDK comes from the
# environment (a Visual Studio developer shell); elsewhere from an `xwin splat` tree, so a Linux host
# compiles and links the Windows build without Visual Studio.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)
set(CMAKE_AR llvm-lib)

if(NOT CMAKE_HOST_WIN32)
    set(OPENSU_XWIN_ROOT "$ENV{HOME}/.local/share/xwin" CACHE PATH "An `xwin splat` output")
    if(NOT EXISTS "${OPENSU_XWIN_ROOT}/crt/include")
        message(FATAL_ERROR
            "No Windows SDK at ${OPENSU_XWIN_ROOT}. Install xwin (`cargo install xwin --locked`) "
            "and run `xwin --accept-license --arch x86_64 splat --output ${OPENSU_XWIN_ROOT}`")
    endif()
    set(_xwin_includes
        "${OPENSU_XWIN_ROOT}/crt/include"
        "${OPENSU_XWIN_ROOT}/sdk/include/ucrt"
        "${OPENSU_XWIN_ROOT}/sdk/include/um"
        "${OPENSU_XWIN_ROOT}/sdk/include/shared"
        "${OPENSU_XWIN_ROOT}/sdk/include/winrt")
    # A link through the compiler driver (meson's checks) uses lld-link too.
    set(_xwin_flags "-fuse-ld=lld")
    set(_xwin_rc_flags "")
    foreach(dir IN LISTS _xwin_includes)
        string(APPEND _xwin_flags " /imsvc \"${dir}\"")
        string(APPEND _xwin_rc_flags " /I \"${dir}\"")
    endforeach()
    set(CMAKE_C_FLAGS_INIT "${_xwin_flags}")
    set(CMAKE_CXX_FLAGS_INIT "${_xwin_flags}")
    set(CMAKE_RC_FLAGS_INIT "${_xwin_rc_flags}")
    set(_xwin_libs
        "/libpath:\"${OPENSU_XWIN_ROOT}/crt/lib/x86_64\""
        "/libpath:\"${OPENSU_XWIN_ROOT}/sdk/lib/um/x86_64\""
        "/libpath:\"${OPENSU_XWIN_ROOT}/sdk/lib/ucrt/x86_64\"")
    list(JOIN _xwin_libs " " _xwin_link)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${_xwin_link}")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_xwin_link}")
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_xwin_link}")
    # xwin carries the release CRT only.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    # The host's own .pc files describe Linux libraries.
    set(ENV{PKG_CONFIG_LIBDIR} "")
endif()
