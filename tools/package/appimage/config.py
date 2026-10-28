"""The AppImage's pins, names and directories, and the environment it reads."""

import os
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
CONTAINERFILE = REPO / "packaging" / "appimage" / "Containerfile"
IMAGE_NAME = "localhost/opensu-appimage-build"

APP_NAME = "openSU"
ARCH = "x86_64"
DESKTOP_ID = "opensu"
UPDATE_INFORMATION = (
    f"gh-releases-zsync|SomeoneIsWorking|opensu|latest|{APP_NAME}-*-{ARCH}.AppImage.zsync"
)

# wlroots and libdisplay-info come from the fork's subprojects: 24.04's are too old. OpenVR is
# off because 24.04 lacks the 2.x SDK and openSU never runs Gamescope in VR mode.
GAMESCOPE_MESON_OPTIONS = (
    "-Dforce_fallback_for=libliftoff,vkroots,wlroots,libdisplay-info -Denable_openvr_support=false"
)

# The floor the build container sets; the bundle check refuses anything newer.
GLIBC_FLOOR = (2, 39)

SIGNING_KEY_ENV = "OPENSU_APPIMAGE_SIGNING_KEY"


@dataclass(frozen=True)
class Download:
    url: str
    sha256: str

    @property
    def name(self) -> str:
        return self.url.rsplit("/", 1)[-1]


UV = Download(
    "https://github.com/astral-sh/uv/releases/download/0.12.19/uv-x86_64-unknown-linux-gnu.tar.gz",
    "23bf5552d220e0842b65c862097b2ebaeba0064b74eda5e565e77fd25969d8c8",
)
UV_VERSION = "0.12.19"
APPIMAGETOOL = Download(
    "https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage",
    "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0",
)
# The static type 2 runtime: no libfuse2 on the host, only fusermount3 (or fusermount).
RUNTIME = Download(
    "https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64",
    "2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d",
)


@dataclass(frozen=True)
class Layout:
    """Everything the AppImage build writes, under one directory of build/."""

    root: Path

    @property
    def deps(self) -> Path:
        return self.root / "deps"

    @property
    def appdir(self) -> Path:
        return self.root / "AppDir"

    @property
    def out(self) -> Path:
        return self.root / "out"

    @property
    def downloads(self) -> Path:
        return self.root / "downloads"

    @property
    def scratch(self) -> Path:
        return self.root / "tmp"

    @property
    def gnupg(self) -> Path:
        return self.root / "gnupg"

    @property
    def smoke(self) -> Path:
        return self.root / "smoke"

    def appimage(self, version: str) -> Path:
        return self.out / f"{APP_NAME}-{version}-{ARCH}.AppImage"


def default_layout() -> Layout:
    return Layout(REPO / "build" / "appimage")


def signing_key() -> str:
    """The armored private key releases are signed with; empty for an unsigned local build."""
    return os.environ.get(SIGNING_KEY_ENV, "")
