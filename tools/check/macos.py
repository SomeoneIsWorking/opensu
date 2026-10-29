"""The macOS build: Homebrew's LLVM Clang, its libraries from vcpkg, MoltenVK as the Vulkan driver.

Runs on an Apple silicon Mac with Xcode's command line tools and Homebrew's `llvm` and `ninja`, as
the `macos-15` runner image has them after `brew install llvm ninja`. Xcode's own Clang is not used:
its libc++ keeps std::jthread experimental. vcpkg is checked out at the manifest's own baseline
commit and builds every port as a dylib through openSU's arm64 triplet (macOS 14 and newer); libcurl
is the system's, which trusts the Keychain. SDL_shadercross, which gpu2d needs for its Metal
shaders, is built by gpu2d's own script against vcpkg's SDL3. The runner's Vulkan driver is MoltenVK
from its pinned, checksummed release; the tests draw through it on the Mac's Metal device.
"""

import json
import platform
import shutil
import sys
import tarfile
from collections.abc import Mapping
from pathlib import Path

from tools import download
from tools.check import process

REPO = Path(__file__).resolve().parents[2]
TRIPLET = "arm64-osx-opensu"
HOST_TRIPLET = "arm64-osx"
TRIPLETS = REPO / "packaging" / "macos" / "triplets"
DEPLOYMENT_TARGET = "14.0"

# vcpkg's host builds of pkgconf and of glslc (gpu2d's shaders), under the build's installed tree.
HOST_TOOLS = Path("vcpkg_installed", HOST_TRIPLET, "tools")
PKGCONF = HOST_TOOLS / "pkgconf" / "pkgconf"
GLSLC = HOST_TOOLS / "shaderc" / "glslc"

MOLTENVK_VERSION = "1.4.2"
MOLTENVK_URL = (
    "https://github.com/KhronosGroup/MoltenVK/releases/download/"
    f"v{MOLTENVK_VERSION}/MoltenVK-macos.tar"
)
MOLTENVK_SHA256 = "f95765a6229cb7b915990a2890ce12ebe36a730b021545d3d52ae69ce4c4024e"
MOLTENVK_DYLIB = "MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib"
MOLTENVK_ICD = "MoltenVK/MoltenVK/dynamic/dylib/macOS/MoltenVK_icd.json"


def require_host() -> None:
    if sys.platform != "darwin" or platform.machine() != "arm64":
        raise RuntimeError(
            "--platform macos builds on an Apple silicon Mac with Xcode's command line tools and "
            "Homebrew (brew install llvm ninja); there is no cross build from another OS"
        )


def llvm_bin(env: Mapping[str, str]) -> Path:
    """Homebrew's LLVM, whose libc++ has std::jthread."""
    if shutil.which("brew", path=env.get("PATH")) is None:
        raise RuntimeError("no Homebrew: see https://brew.sh, then brew install llvm ninja")
    prefix = Path(process.output(["brew", "--prefix", "llvm"], env).strip())
    if not (prefix / "bin" / "clang++").is_file():
        raise RuntimeError("Homebrew's LLVM is not installed: brew install llvm")
    return prefix / "bin"


def environment(build_dir: Path, env: Mapping[str, str]) -> dict[str, str]:
    """The build's environment: Homebrew's LLVM first on PATH (clang-tidy too), one macOS SDK for
    every compiler and CMake find (`SDKROOT`), and vcpkg's binary cache under the build directory
    unless the caller names one."""
    merged = dict(env)
    merged["PATH"] = f"{llvm_bin(env)}:{merged.get('PATH', '')}"
    # Homebrew's Clang and CMake's finds otherwise pick different SDKs, and an SDK's usr/include
    # on the include path hides libc++'s own C headers.
    merged["SDKROOT"] = process.output(["xcrun", "--sdk", "macosx", "--show-sdk-path"], env).strip()
    if shutil.which("ninja", path=merged["PATH"]) is None:
        raise RuntimeError("ninja is not on PATH: brew install ninja")
    if "VCPKG_BINARY_SOURCES" not in merged:
        cache = build_dir / "vcpkg-cache"
        cache.mkdir(parents=True, exist_ok=True)
        merged["VCPKG_BINARY_SOURCES"] = f"clear;files,{cache},readwrite"
    merged["VCPKG_DISABLE_METRICS"] = "1"
    return merged


def bootstrap(vcpkg: Path, env: Mapping[str, str]) -> None:
    """vcpkg's own tool, which its bootstrap script downloads and checks by SHA-512."""
    if (vcpkg / "vcpkg").is_file():
        return
    process.run([vcpkg / "bootstrap-vcpkg.sh", "-disableMetrics"], env)


def installed(build_dir: Path) -> Path:
    return build_dir / "vcpkg_installed"


def install_ports(vcpkg: Path, build_dir: Path, env: Mapping[str, str]) -> None:
    """The manifest's ports, before configuring, since SDL_shadercross is built against SDL3."""
    process.run(
        [vcpkg / "vcpkg", "install", f"--triplet={TRIPLET}", f"--host-triplet={HOST_TRIPLET}",
         f"--overlay-triplets={TRIPLETS}", f"--x-manifest-root={REPO}",
         f"--x-install-root={installed(build_dir)}"],
        env,
    )  # fmt: skip


def shadercross(gpu2d: Path, build_dir: Path, env: Mapping[str, str]) -> Path:
    """gpu2d's pinned SDL_shadercross, built once into the build directory."""
    prefix = build_dir / "shadercross"
    process.run(
        ["cmake", f"-DPREFIX={prefix}", "-P", gpu2d / "cmake" / "ProvisionShadercross.cmake"],
        {**env, "CMAKE_PREFIX_PATH": str(installed(build_dir) / TRIPLET)},
    )
    return prefix / "bin" / "shadercross"


def configure_defines(vcpkg: Path, build_dir: Path, env: Mapping[str, str]) -> list[str]:
    clang = llvm_bin(env)
    return [
        f"-DCMAKE_TOOLCHAIN_FILE={vcpkg / 'scripts' / 'buildsystems' / 'vcpkg.cmake'}",
        f"-DVCPKG_TARGET_TRIPLET={TRIPLET}",
        f"-DVCPKG_HOST_TRIPLET={HOST_TRIPLET}",
        f"-DVCPKG_OVERLAY_TRIPLETS={TRIPLETS}",
        f"-DVCPKG_INSTALLED_DIR={installed(build_dir)}",
        f"-DCMAKE_C_COMPILER={clang / 'clang'}",
        f"-DCMAKE_CXX_COMPILER={clang / 'clang++'}",
        "-DCMAKE_OSX_ARCHITECTURES=arm64",
        f"-DCMAKE_OSX_DEPLOYMENT_TARGET={DEPLOYMENT_TARGET}",
        f"-DCMAKE_OSX_SYSROOT={env['SDKROOT']}",
        # The SDL3, Vulkan, Vorbis and SQLite modules are found through their .pc files.
        f"-DPKG_CONFIG_EXECUTABLE={build_dir / PKGCONF}",
        f"-DGLSLC={build_dir / GLSLC}",
        f"-DGPU2D_SHADERCROSS={build_dir / 'shadercross' / 'bin' / 'shadercross'}",
    ]


def moltenvk(root: Path) -> Path:
    """MoltenVK's dylib and its ICD manifest unpacked under `root`; returns the manifest."""
    archive = download.fetch_verified(
        MOLTENVK_URL, MOLTENVK_SHA256, root / f"MoltenVK-macos-{MOLTENVK_VERSION}.tar"
    )
    unpacked = root / f"moltenvk-{MOLTENVK_VERSION}"
    icd = unpacked / MOLTENVK_ICD
    if not icd.is_file():
        with tarfile.open(archive) as tar:
            for name in (MOLTENVK_DYLIB, MOLTENVK_ICD):
                tar.extract(name, unpacked, filter="data")
    manifest = json.loads(icd.read_text())
    if manifest["ICD"]["library_path"] != "./libMoltenVK.dylib":
        raise RuntimeError(f"{icd} names {manifest['ICD']['library_path']}, not its own dylib")
    return icd


def with_moltenvk(root: Path, env: Mapping[str, str]) -> dict[str, str]:
    """`env` with MoltenVK as the only Vulkan driver."""
    icd = moltenvk(root)
    return {**env, "VK_DRIVER_FILES": str(icd), "VK_ICD_FILENAMES": str(icd)}
