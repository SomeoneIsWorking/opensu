"""Third-party sources openSU builds against, pinned by commit."""

from dataclasses import dataclass


@dataclass(frozen=True)
class GitPin:
    name: str
    url: str
    commit: str


# raylib 6.0 plus the fork's miniaudio PulseAudio fix. The published binary has no image decoding.
RAYLIB = GitPin(
    "raylib",
    "https://github.com/SomeoneIsWorking/raylib.git",
    "7503d6557c3fa8796994e8a1a673556e437143e6",
)
LUCENT = GitPin(
    "lucent",
    "https://github.com/SomeoneIsWorking/lucent.git",
    "efb9a7bdacaf7c04663a225233dc7da125c491f3",
)

# Image decoding on and X11 only: openSU reads its X11 window to become Gamescope's overlay.
RAYLIB_CMAKE_OPTIONS = (
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_EXAMPLES=OFF",
    "-DGLFW_BUILD_WAYLAND=OFF",
    "-DGLFW_BUILD_X11=ON",
    "-DCMAKE_INSTALL_LIBDIR=lib",
    "-DCMAKE_C_FLAGS=-DSTBI_REQUIRED -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_TGA=1"
    " -DSUPPORT_FILEFORMAT_PSD=1",
)
