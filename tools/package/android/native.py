"""The shared native prefix and openSU's libraries per ABI: libmain.so, libopensu-retro.so and
librashader.so."""

import os
from collections.abc import Mapping
from pathlib import Path
from types import ModuleType

from tools.check import process
from tools.check.deps import Deps
from tools.package.android import config

TARGETS = ("opensu", "opensu-retro", "opensu_librashader")


def ensure_prefix(layout: config.Layout, port: ModuleType, abi: str) -> object:
    """The shared prefix for `abi`, built once when it is missing or lacks a capability; returns
    the profile that names it."""
    link = layout.deps_link(abi)
    link.parent.mkdir(parents=True, exist_ok=True)
    if link.is_symlink() or link.exists():
        link.unlink()
    layout.prefix(abi).mkdir(parents=True, exist_ok=True)
    link.symlink_to(layout.prefix(abi), target_is_directory=True)
    profile = port.load_android_port_profile(layout.profile(abi))
    try:
        port.validate_native_dependency_prefix(profile)
    except SystemExit as missing:
        print(f"building the {abi} prefix: {missing}", flush=True)
        port.build_native_dependencies(
            port.native_dependency_request_for_profile(profile, layout.ndk), os.cpu_count() or 4
        )
        port.validate_native_dependency_prefix(profile)
    return profile


def build(layout: config.Layout, sources: Deps, abi: str, env: Mapping[str, str]) -> Path:
    """Configure and build both libraries for `abi`; returns the build directory."""
    prefix = layout.prefix(abi)
    build_dir = layout.native_dir(abi)
    toolchain = layout.ndk / "build" / "cmake" / "android.toolchain.cmake"
    native_env = {
        **env,
        # Only the prefix's packages, linked statically with what they need.
        "PKG_CONFIG_LIBDIR": str(prefix / "lib" / "pkgconfig"),
        "PKG_CONFIG_PATH": "",
    }
    process.run(
        [
            "cmake",
            "-S",
            config.REPO,
            "-B",
            build_dir,
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DANDROID_ABI={abi}",
            f"-DANDROID_PLATFORM=android-{config.API_LEVEL}",
            "-DANDROID_STL=c++_shared",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_FIND_ROOT_PATH={prefix}",
            "-DPKG_CONFIG_ARGN=--static",
            # std::jthread and std::stop_token are experimental in the NDK's libc++.
            "-DCMAKE_CXX_FLAGS=-fexperimental-library",
            f"-DLUCENT_ROOT={sources.lucent}",
            f"-DGPU2D_ROOT={sources.gpu2d}",
            "-DOPENSU_BUILD_GAMESCOPE=OFF",
            "-DOPENSU_BUILD_LIBRASHADER=ON",
            # The NDK's own shader compiler, so the host needs none.
            f"-DGLSLC={layout.ndk / 'shader-tools' / 'linux-x86_64' / 'glslc'}",
        ],
        native_env,
    )
    process.run(["cmake", "--build", build_dir, "--target", *TARGETS], native_env)
    return build_dir
