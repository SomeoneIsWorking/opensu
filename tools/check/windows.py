"""The Windows build: clang-cl with the MSVC STL and the Windows SDK, its libraries from vcpkg.

Runs on a Windows host with Visual Studio (its C++ tools and SDK) and LLVM installed, as the
`windows-2025` runner image has them. vcpkg is checked out at the manifest's own baseline commit,
so the commit is the checksum, and builds every port through openSU's clang-cl triplet. The tests'
hidden windows draw through Mesa's lavapipe from a pinned mesa-dist-win release, since a hosted
runner has no GPU.
"""

import json
import shutil
import subprocess
import sys
from collections.abc import Mapping
from pathlib import Path

from tools import download
from tools.check import process
from tools.check.pins import GitPin

REPO = Path(__file__).resolve().parents[2]
TRIPLET = "x64-windows-clangcl"
TOOLCHAIN = REPO / "cmake" / "toolchains" / "windows-clang-cl.cmake"
TRIPLETS = REPO / "packaging" / "windows" / "triplets"

# vcpkg's host builds of pkgconf and of glslc (gpu2d's shaders), under the build's installed tree.
HOST_TOOLS = Path("vcpkg_installed", "x64-windows", "tools")
PKGCONF = HOST_TOOLS / "pkgconf" / "pkgconf.exe"
GLSLC = HOST_TOOLS / "shaderc" / "glslc.exe"

VSWHERE = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")

LAVAPIPE_VERSION = "26.2.3"
LAVAPIPE_URL = (
    "https://github.com/pal1000/mesa-dist-win/releases/download/"
    f"{LAVAPIPE_VERSION}/mesa3d-{LAVAPIPE_VERSION}-release-msvc.7z"
)
LAVAPIPE_SHA256 = "3f3613adb43cfd0f2e665ce2400b130c275f0b3317cb3a05566320a3a67589ed"
LAVAPIPE_ICD = "lvp_icd.x86_64.json"


def require_host() -> None:
    if sys.platform != "win32":
        raise RuntimeError(
            "--platform windows builds on a Windows host with Visual Studio and LLVM; from Linux, "
            "cmake/toolchains/windows-clang-cl.cmake compiles against an xwin SDK, but vcpkg's "
            "OpenSSL needs nmake"
        )


def vcpkg_pin() -> GitPin:
    """vcpkg at vcpkg.json's builtin-baseline: the ports the manifest resolves are that commit's."""
    baseline = json.loads((REPO / "vcpkg.json").read_text())["builtin-baseline"]
    return GitPin("vcpkg", "https://github.com/microsoft/vcpkg.git", baseline)


def bootstrap(vcpkg: Path, env: Mapping[str, str]) -> None:
    """vcpkg's own tool, which its bootstrap script downloads and checks by SHA-512."""
    if (vcpkg / "vcpkg.exe").is_file():
        return
    process.run(["cmd", "/d", "/c", str(vcpkg / "bootstrap-vcpkg.bat"), "-disableMetrics"], env)


def msvc_environment(env: Mapping[str, str]) -> dict[str, str]:
    """`env` with the Visual Studio x64 developer environment (INCLUDE, LIB, the SDK's tools)."""
    if not VSWHERE.is_file():
        raise RuntimeError(
            f"no Visual Studio: {VSWHERE} is missing; install Visual Studio 2022 "
            "with the C++ desktop workload"
        )
    found = process.output(
        [VSWHERE, "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        env,
    ).strip()  # fmt: skip
    if not found:
        raise RuntimeError("Visual Studio has no C++ x64 tools; add the C++ desktop workload")
    vcvars = Path(found) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    listing = subprocess.run(
        f'cmd /d /c call "{vcvars}" >nul && set',
        env=dict(env),
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    # Windows names are case-insensitive: vcvars' `Path` replaces the caller's `PATH`, never sits
    # beside it, or a child would see either one.
    merged = {name.upper(): value for name, value in env.items()}
    for line in listing.splitlines():
        name, sep, value = line.partition("=")
        if sep and name:
            merged[name.upper()] = value
    for tool in ("clang-cl", "lld-link", "ninja"):
        if shutil.which(tool, path=merged.get("PATH")) is None:
            raise RuntimeError(f"{tool} is not on PATH; install LLVM (winget install LLVM.LLVM)")
    return merged


def environment(build_dir: Path, env: Mapping[str, str]) -> dict[str, str]:
    """The build's environment: MSVC's, and vcpkg's binary cache under the build directory unless
    the caller names one."""
    merged = msvc_environment(env)
    if "VCPKG_BINARY_SOURCES" not in merged:
        cache = build_dir / "vcpkg-cache"
        cache.mkdir(parents=True, exist_ok=True)
        merged["VCPKG_BINARY_SOURCES"] = f"clear;files,{cache},readwrite"
    merged["VCPKG_DISABLE_METRICS"] = "1"
    return merged


def configure_defines(vcpkg: Path, build_dir: Path) -> list[str]:
    return [
        f"-DCMAKE_TOOLCHAIN_FILE={vcpkg / 'scripts' / 'buildsystems' / 'vcpkg.cmake'}",
        f"-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE={TOOLCHAIN}",
        f"-DVCPKG_TARGET_TRIPLET={TRIPLET}",
        f"-DVCPKG_OVERLAY_TRIPLETS={TRIPLETS}",
        f"-DVCPKG_INSTALLED_DIR={build_dir / 'vcpkg_installed'}",
        # The SDL3, Vulkan, Vorbis and SQLite modules are found through their .pc files.
        f"-DPKG_CONFIG_EXECUTABLE={build_dir / PKGCONF}",
        f"-DGLSLC={build_dir / GLSLC}",
        # The DLLs each executable imports are copied beside it in the build and on install.
        "-DVCPKG_APPLOCAL_DEPS=ON",
        "-DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON",
    ]


def lavapipe(root: Path, env: Mapping[str, str]) -> dict[str, str]:
    """`env` with Mesa's lavapipe as the only Vulkan driver."""
    archive = download.fetch_verified(
        LAVAPIPE_URL, LAVAPIPE_SHA256, root / f"mesa3d-{LAVAPIPE_VERSION}-release-msvc.7z"
    )
    unpacked = root / f"mesa3d-{LAVAPIPE_VERSION}"
    icd = unpacked / "x64" / LAVAPIPE_ICD
    if not icd.is_file():
        process.run(["7z", "x", "-y", f"-o{unpacked}", archive, "x64"], env)
    if not icd.is_file():
        raise RuntimeError(f"{archive} holds no x64/{LAVAPIPE_ICD}")
    return {**env, "VK_DRIVER_FILES": str(icd), "VK_ICD_FILENAMES": str(icd)}
