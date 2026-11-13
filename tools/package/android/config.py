"""Pins, paths and the environment the Android package is built from."""

import json
import os
from dataclasses import dataclass
from pathlib import Path

from tools.checkout import REPO

SOURCE = REPO / "packaging" / "android"
DEFAULT_BUILD = REPO / "build" / "android"

NDK_VERSION = "28.2.13676358"
BUILD_TOOLS = "35.0.0"
PLATFORM = "android-35"
# posix_spawn and the Vulkan loader both need API 28.
API_LEVEL = 28
# The shared prefix is built for the android-port floor and links into any later API.
PREFIX_API = 21
ABIS = ("arm64-v8a", "x86_64")
APPLICATION_ID = "io.github.someoneisworking.opensu"

# The release key's certificate (OPENSU_ANDROID_KEYSTORE); every published APK carries it.
RELEASE_CERT_SHA256 = "BDC838D582DDC695CDAB431D2053B636EB93A0C2E1C3F468DD002D1011E43238"

SIGNING_ENV = (
    "OPENSU_ANDROID_KEYSTORE",
    "OPENSU_ANDROID_KEYSTORE_PASSWORD",
    "OPENSU_ANDROID_KEY_ALIAS",
    "OPENSU_ANDROID_KEY_PASSWORD",
)


@dataclass(frozen=True)
class Layout:
    """The SDK, NDK and shared native prefix this build uses, and the directory it builds in."""

    sdk: Path
    deps_root: Path
    root: Path = DEFAULT_BUILD

    @property
    def ndk(self) -> Path:
        return self.sdk / "ndk" / NDK_VERSION

    @property
    def sources(self) -> Path:
        """Pinned checkouts: Lucent, gpu2d and android-port."""
        return self.root / "sources"

    @property
    def android_port(self) -> Path:
        return self.sources / "android-port"

    @property
    def project(self) -> Path:
        return self.root / "project"

    @property
    def out(self) -> Path:
        return self.root / "out"

    @property
    def scratch(self) -> Path:
        return self.root / "tmp"

    def native_dir(self, abi: str) -> Path:
        return self.root / abi

    def deps_link(self, abi: str) -> Path:
        """The profile's prefix path, a link to the shared prefix."""
        return self.root / "deps" / abi

    def native_library(self, abi: str) -> Path:
        return self.native_dir(abi) / "src" / "libmain.so"

    @property
    def jni_libs(self) -> Path:
        return self.project / "app" / "src" / "main" / "jniLibs"

    def prefix(self, abi: str) -> Path:
        return self.deps_root / abi

    def profile(self, abi: str) -> Path:
        """The android-port profile this build reads for `abi` (`write_profile`)."""
        return self.root / "profiles" / f"{abi}.json"

    def write_profile(self, abi: str) -> Path:
        """packaging/android/profiles/<abi>.json's policy with this build's paths, each relative to
        the written profile as android-port takes them."""
        source = SOURCE / "profiles" / f"{abi}.json"
        profile = json.loads(source.read_text(encoding="utf-8"))
        path = self.profile(abi)

        def relative(target: Path) -> str:
            return os.path.relpath(target, path.parent)

        profile["nativeDependencies"]["prefix"] = relative(self.deps_link(abi))
        profile["package"] = {
            "nativeLibrary": relative(self.native_library(abi)),
            "jniLibs": relative(self.jni_libs),
        }
        lock = (source.parent / profile["sharedEmulator"]["lock"]).resolve()
        profile["sharedEmulator"]["lock"] = relative(lock)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")
        return path


def refuse(message: str) -> SystemExit:
    return SystemExit(f"tools.package.android: {message}")


def layout_from_environment(root: Path = DEFAULT_BUILD) -> Layout:
    """ANDROID_HOME (or ANDROID_SDK_ROOT) and OPENSU_ANDROID_DEPS, the shared prefix's root, for
    a build in `root`."""
    sdk_value = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if not sdk_value or not Path(sdk_value).is_dir():
        raise refuse(
            "set ANDROID_HOME to an Android SDK with "
            f"ndk;{NDK_VERSION}, build-tools;{BUILD_TOOLS} and platforms;{PLATFORM}"
        )
    sdk = Path(sdk_value).resolve()
    if not (sdk / "ndk" / NDK_VERSION).is_dir():
        raise refuse(f"the SDK has no NDK {NDK_VERSION}: sdkmanager 'ndk;{NDK_VERSION}'")
    home = Path.home()
    deps = os.environ.get("OPENSU_ANDROID_DEPS") or str(
        home / ".local/share/android-port-deps" / f"ndk-{NDK_VERSION}" / f"android-{PREFIX_API}"
    )
    return Layout(sdk=sdk, deps_root=Path(deps).resolve(), root=root.resolve())
