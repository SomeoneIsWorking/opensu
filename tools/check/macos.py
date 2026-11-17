"""The macOS build: Xcode's Clang, its libraries from vcpkg, MoltenVK as the Vulkan driver.

Runs on an Apple silicon Mac with Xcode's command line tools (Apple Clang 21 or newer, whose libc++
has std::jthread) and Ninja, as the `macos-26` runner image has them. Xcode ships no clang-tidy, so
only `lint` needs one: Homebrew's `llvm@22`, the Linux gate's LLVM, when it is installed, else the
one on PATH, else lint is refused with the install command. vcpkg is checked out at the manifest's
own baseline commit and builds every port as a dylib through openSU's arm64 triplet (macOS 14 and
newer); libcurl
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
from tools.checkout import REPO

TRIPLET = "arm64-osx-opensu"
HOST_TRIPLET = "arm64-osx"
TRIPLETS = REPO / "packaging" / "macos" / "triplets"
DEPLOYMENT_TARGET = "14.0"
# The LLVM major of the Linux gate (Fedora 44): lint's clang-tidy, the same checks on both trees.
LLVM_FORMULA = "llvm@22"
# The oldest Apple Clang major whose libc++ has std::jthread without -fexperimental-library.
MIN_APPLE_CLANG = 21

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
            "Ninja (brew install ninja); there is no cross build from another OS"
        )


def xcode_clang(env: Mapping[str, str]) -> Path:
    """Xcode's clang++, refused when its libc++ may keep std::jthread experimental."""
    clang = Path(process.output(["xcrun", "--find", "clang++"], env).strip())
    version = process.output([clang, "--version"], env)
    words = version.split()
    major = (
        int(words[3].split(".")[0])
        if words[:3] == ["Apple", "clang", "version"] and words[3].split(".")[0].isdigit()
        else 0
    )
    if major < MIN_APPLE_CLANG:
        raise RuntimeError(
            f"{clang} is {version.splitlines()[0]}; openSU needs Apple Clang {MIN_APPLE_CLANG} or "
            "newer for std::jthread: update Xcode's command line tools "
            "(softwareupdate --list, or xcode-select --install)"
        )
    return clang


def clang_tidy_environment(env: Mapping[str, str]) -> dict[str, str]:
    """`env` with a clang-tidy for lint, which Xcode does not ship: Homebrew's `llvm@22` first, the
    Linux gate's, else one already on PATH; refused with the install command when there is none."""
    merged = dict(env)
    if shutil.which("brew", path=env.get("PATH")) is not None:
        prefix = Path(process.output(["brew", "--prefix", LLVM_FORMULA], env).strip())
        if (prefix / "bin" / "clang-tidy").is_file():
            merged["PATH"] = f"{prefix / 'bin'}:{merged.get('PATH', '')}"
            print(f"+ clang-tidy from Homebrew's {LLVM_FORMULA}", flush=True)
            return merged
    found = shutil.which("clang-tidy", path=env.get("PATH"))
    if found is None:
        raise RuntimeError(
            "lint on macOS needs clang-tidy, which Xcode does not ship: "
            f"brew install {LLVM_FORMULA}"
        )
    print(f"+ clang-tidy from {found}, not the Linux gate's {LLVM_FORMULA}", flush=True)
    return merged


def environment(build_dir: Path, env: Mapping[str, str]) -> dict[str, str]:
    """The build's environment: one macOS SDK for every compiler and CMake find (`SDKROOT`), and
    vcpkg's binary cache under the build directory unless the caller names one."""
    merged = dict(env)
    # The compilers and CMake's finds see the same SDK.
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


def compiler_defines(env: Mapping[str, str]) -> list[str]:
    """Xcode's Clang for arm64 at the deployment target, on the one SDK."""
    clangxx = xcode_clang(env)
    return [
        f"-DCMAKE_C_COMPILER={clangxx.with_name('clang')}",
        f"-DCMAKE_CXX_COMPILER={clangxx}",
        f"-DCMAKE_OBJCXX_COMPILER={clangxx}",
        "-DCMAKE_OSX_ARCHITECTURES=arm64",
        f"-DCMAKE_OSX_DEPLOYMENT_TARGET={DEPLOYMENT_TARGET}",
        f"-DCMAKE_OSX_SYSROOT={env['SDKROOT']}",
    ]


def configure_defines(vcpkg: Path, build_dir: Path, env: Mapping[str, str]) -> list[str]:
    return [
        f"-DCMAKE_TOOLCHAIN_FILE={vcpkg / 'scripts' / 'buildsystems' / 'vcpkg.cmake'}",
        f"-DVCPKG_TARGET_TRIPLET={TRIPLET}",
        f"-DVCPKG_HOST_TRIPLET={HOST_TRIPLET}",
        f"-DVCPKG_OVERLAY_TRIPLETS={TRIPLETS}",
        f"-DVCPKG_INSTALLED_DIR={installed(build_dir)}",
        *compiler_defines(env),
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
