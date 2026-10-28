"""Pins, paths and the environment the Android package is built from."""

import os
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
SOURCE = REPO / "packaging" / "android"
BUILD = REPO / "build" / "android"

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
    """The SDK, NDK and shared native prefix this build uses."""

    sdk: Path
    deps_root: Path

    @property
    def ndk(self) -> Path:
        return self.sdk / "ndk" / NDK_VERSION

    @property
    def sources(self) -> Path:
        """Pinned checkouts: Lucent, gpu2d and android-port."""
        return BUILD / "sources"

    @property
    def android_port(self) -> Path:
        return self.sources / "android-port"

    @property
    def project(self) -> Path:
        return BUILD / "project"

    @property
    def out(self) -> Path:
        return BUILD / "out"

    @property
    def scratch(self) -> Path:
        return BUILD / "tmp"

    def native_dir(self, abi: str) -> Path:
        return BUILD / abi

    def deps_link(self, abi: str) -> Path:
        """The profile's prefix path, a link to the shared prefix."""
        return BUILD / "deps" / abi

    def prefix(self, abi: str) -> Path:
        return self.deps_root / abi

    def profile(self, abi: str) -> Path:
        return SOURCE / "profiles" / f"{abi}.json"


def refuse(message: str) -> SystemExit:
    return SystemExit(f"tools.package.android: {message}")


def layout_from_environment() -> Layout:
    """ANDROID_HOME (or ANDROID_SDK_ROOT) and OPENSU_ANDROID_DEPS, the shared prefix's root."""
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
    return Layout(sdk=sdk, deps_root=Path(deps).resolve())
