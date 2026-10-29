# librashader, which runs libretro slang shader presets in opensu-retro: the C API of the pinned
# fork, built with cargo (Vulkan runtime only) as an ExternalProject and staged where the runner
# loads it at run time, `<prefix>/<libdir>/opensu/librashader.so` (`.dll` on Windows,
# `Contents/Frameworks/librashader.dylib` in the macOS app, `Config::librashader`), in the build
# tree and installed. A Windows build targets
# x86_64-pc-windows-msvc with clang-cl and lld-link, also from a Linux host through the xwin SDK; an
# Android build targets the ABI's Rust target with the NDK's clang and is packaged beside the
# runner as librashader.so. The runtime is MPL-2.0 (or GPL-3); its C header librashader.h is MIT
# and comes from the same commit, checked by SHA-256. Static linking is not supported upstream.
#
# OPENSU_BUILD_LIBRASHADER=OFF builds without it, for a machine with no Rust 1.88: the runner then
# refuses every shader preset with the reason. A pin change fetches the commit into the existing
# clone; an unchanged pin runs nothing.

include(ExternalProject)
include(FetchContent)
include(GNUInstallDirs)

option(OPENSU_BUILD_LIBRASHADER
    "Build the pinned librashader fork that runs shader presets. OFF builds opensu-retro without \
it: a shader preset is then refused" ON)

set(OPENSU_LIBRASHADER_REPOSITORY "https://github.com/SomeoneIsWorking/librashader.git")
set(OPENSU_LIBRASHADER_COMMIT "41211fab77a19f4387d55ffffcb713ca1db7cf32")
set(OPENSU_LIBRASHADER_HEADER_SHA256
    "5d478897c391af3f60015810b67785ae1a286d262a845485276e36ded9f21e62")
# The release the fork's MSRV names (Rust 1.88 without wgpu).
set(OPENSU_LIBRASHADER_RUST_MINIMUM "1.88")

# Where the runner finds the library, relative to the directory above its own: openSU's own
# folder in the library directory, or the app bundle's Frameworks on macOS.
if(APPLE)
    set(OPENSU_LIBRASHADER_DIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(OPENSU_LIBRASHADER_DIR "${CMAKE_INSTALL_LIBDIR}/opensu")
endif()
set(OPENSU_LIBRASHADER_RELATIVE
    "${OPENSU_LIBRASHADER_DIR}/librashader${CMAKE_SHARED_LIBRARY_SUFFIX}")

FetchContent_Declare(librashader_header
    URL "https://raw.githubusercontent.com/SomeoneIsWorking/librashader/${OPENSU_LIBRASHADER_COMMIT}/include/librashader.h"
    URL_HASH "SHA256=${OPENSU_LIBRASHADER_HEADER_SHA256}"
    DOWNLOAD_NO_EXTRACT TRUE
    SOURCE_SUBDIR "no-cmake"
)
FetchContent_MakeAvailable(librashader_header)
add_library(opensu_librashader_header INTERFACE)
add_library(opensu::librashader_header ALIAS opensu_librashader_header)
target_include_directories(opensu_librashader_header SYSTEM INTERFACE
    "${librashader_header_SOURCE_DIR}")

# cargo, refused below the fork's MSRV.
function(opensu_librashader_cargo out)
    find_program(OPENSU_CARGO cargo HINTS "$ENV{CARGO_HOME}/bin" "$ENV{HOME}/.cargo/bin")
    set(install_hint
        "Install Rust ${OPENSU_LIBRASHADER_RUST_MINIMUM} or newer: `sudo dnf install cargo` \
(Fedora), or rustup (https://rustup.rs) on Debian/Ubuntu, whose packaged Rust is older. To build \
without shader presets, configure with -DOPENSU_BUILD_LIBRASHADER=OFF.")
    if(NOT OPENSU_CARGO)
        message(FATAL_ERROR "librashader needs cargo. ${install_hint}")
    endif()
    execute_process(COMMAND "${OPENSU_CARGO}" --version
        OUTPUT_VARIABLE version RESULT_VARIABLE failed OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(failed OR NOT version MATCHES "^cargo ([0-9]+\\.[0-9]+)")
        message(FATAL_ERROR "${OPENSU_CARGO} --version failed (${version}). ${install_hint}")
    endif()
    if(CMAKE_MATCH_1 VERSION_LESS OPENSU_LIBRASHADER_RUST_MINIMUM)
        message(FATAL_ERROR "librashader needs Rust ${OPENSU_LIBRASHADER_RUST_MINIMUM}; this is "
            "${version}. ${install_hint}")
    endif()
    get_filename_component(cargo_dir "${OPENSU_CARGO}" DIRECTORY)
    find_program(OPENSU_RUSTC rustc HINTS "${cargo_dir}" NO_DEFAULT_PATH)
    find_program(OPENSU_RUSTC rustc)
    execute_process(COMMAND "${OPENSU_RUSTC}" -vV
        OUTPUT_VARIABLE rustc_info RESULT_VARIABLE failed)
    if(failed OR NOT rustc_info MATCHES "host: ([^\n]+)")
        message(FATAL_ERROR "${OPENSU_RUSTC} -vV failed (${rustc_info}). ${install_hint}")
    endif()
    string(TOUPPER "${CMAKE_MATCH_1}" host)
    string(REPLACE "-" "_" host "${host}")
    set(${out} "${OPENSU_CARGO}" PARENT_SCOPE)
    # Cargo links build scripts with `cc` unless told otherwise; images with only Clang have none.
    set(${out}_linker_variable "CARGO_TARGET_${host}_LINKER" PARENT_SCOPE)
endfunction()

function(opensu_add_librashader)
    find_package(Git REQUIRED)
    opensu_librashader_cargo(cargo)
    set(root "${CMAKE_BINARY_DIR}/librashader")
    set(library
        "${CMAKE_SHARED_LIBRARY_PREFIX}librashader_capi${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(staged "${CMAKE_BINARY_DIR}/${OPENSU_LIBRASHADER_RELATIVE}")
    set(pin_file "${root}/pin.txt")
    file(CONFIGURE OUTPUT "${pin_file}" CONTENT "${OPENSU_LIBRASHADER_COMMIT}\n")

    if(WIN32)
        # The glslang and SPIRV-Cross crates build with the target's clang-cl; the host's own
        # compiler and linker stay cargo's defaults for build scripts.
        set(rust_target "x86_64-pc-windows-msvc")
        set(built "${root}/target/${rust_target}/optimized/${library}")
        set(cargo_env
            "CC_x86_64_pc_windows_msvc=${CMAKE_C_COMPILER}"
            "CXX_x86_64_pc_windows_msvc=${CMAKE_CXX_COMPILER}"
            # SPIRV-Cross throws; clang-cl, unlike cl, refuses `try` without /EHsc.
            "CXXFLAGS_x86_64_pc_windows_msvc=/EHsc"
            "CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER=${CMAKE_LINKER}")
        # From a Linux host the SDK is the xwin tree, which clang-cl and lld-link read from
        # INCLUDE and LIB.
        if(OPENSU_WINDOWS_INCLUDE_DIRS)
            list(JOIN OPENSU_WINDOWS_INCLUDE_DIRS "|" include_dirs)
            list(JOIN OPENSU_WINDOWS_LIB_DIRS "|" lib_dirs)
            list(APPEND cargo_env "INCLUDE=${include_dirs}" "LIB=${lib_dirs}")
        endif()
        set(target_args --target "${rust_target}")
    elseif(ANDROID)
        # The NDK's API-level clang wrappers compile the crates' C and C++ and link the library;
        # build scripts stay on the host's own linker.
        if(ANDROID_ABI STREQUAL "arm64-v8a")
            set(rust_target "aarch64-linux-android")
        elseif(ANDROID_ABI STREQUAL "x86_64")
            set(rust_target "x86_64-linux-android")
        else()
            message(FATAL_ERROR "librashader has no Rust target for ${ANDROID_ABI}")
        endif()
        string(TOUPPER "${rust_target}" target_variable)
        string(REPLACE "-" "_" target_variable "${target_variable}")
        string(REPLACE "-" "_" target_suffix "${rust_target}")
        set(clang "${ANDROID_TOOLCHAIN_ROOT}/bin/${rust_target}${ANDROID_PLATFORM_LEVEL}-clang")
        set(built "${root}/target/${rust_target}/optimized/${library}")
        set(cargo_env
            "CC_${target_suffix}=${clang}"
            "CXX_${target_suffix}=${clang}++"
            "AR_${target_suffix}=${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar"
            "CARGO_TARGET_${target_variable}_LINKER=${clang}")
        set(target_args --target "${rust_target}")
    else()
        set(built "${root}/target/optimized/${library}")
        # The glslang and SPIRV-Cross crates compile C and C++ with the project's compilers.
        set(cargo_env "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
            "${cargo_linker_variable}=${CMAKE_C_COMPILER}")
        set(target_args "")
    endif()

    ExternalProject_Add(opensu_librashader
        GIT_REPOSITORY "${OPENSU_LIBRASHADER_REPOSITORY}"
        GIT_TAG "${OPENSU_LIBRASHADER_COMMIT}"
        GIT_SUBMODULES ""
        GIT_PROGRESS ON
        UPDATE_DISCONNECTED ON
        SOURCE_DIR "${root}/source"
        BINARY_DIR "${root}/source"
        STAMP_DIR "${root}/stamp"
        TMP_DIR "${root}/tmp"
        DOWNLOAD_DIR "${root}/download"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND "${CMAKE_COMMAND}" -E env ${cargo_env} "CARGO_TARGET_DIR=${root}/target"
            "${cargo}" build --locked --manifest-path "${root}/source/Cargo.toml"
            --package librashader-capi --profile optimized --no-default-features
            --features runtime-vulkan ${target_args}
        BUILD_BYPRODUCTS "${built}"
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${built}" "${staged}"
        BUILD_ALWAYS OFF
        LIST_SEPARATOR "|"
        USES_TERMINAL_DOWNLOAD ON
        USES_TERMINAL_BUILD ON
    )
    # The disconnected update refuses a commit the clone does not have.
    ExternalProject_Add_Step(opensu_librashader fetch_pin
        COMMAND "${CMAKE_COMMAND}" "-DGIT=${GIT_EXECUTABLE}" "-DSOURCE=${root}/source"
            "-DCOMMIT=${OPENSU_LIBRASHADER_COMMIT}" -DNAME=librashader
            -P "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDS "${pin_file}" "${CMAKE_SOURCE_DIR}/cmake/GitFetchPin.cmake"
        DEPENDEES download
        DEPENDERS update_disconnected
        INDEPENDENT TRUE
        USES_TERMINAL ON)

    install(FILES "${staged}" DESTINATION "${OPENSU_LIBRASHADER_DIR}")
endfunction()

if(OPENSU_BUILD_LIBRASHADER)
    opensu_add_librashader()
endif()
