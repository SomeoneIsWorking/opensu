"""Names and places of the macOS package."""

from dataclasses import dataclass
from pathlib import Path

from tools.check import macos

REPO = Path(__file__).resolve().parents[3]
APP_NAME = "openSU"
ARCH = "arm64"
INFO_PLIST = REPO / "packaging" / "macos" / "Info.plist.in"
# The executables CMake installs into Contents/MacOS.
EXECUTABLES = ("opensu", "opensu-retro")
# Where the Vulkan loader looks for drivers inside the running app's bundle.
ICD_FOLDER = Path("Resources", "vulkan", "icd.d")


@dataclass(frozen=True)
class Layout:
    """Everything the package writes, under the macOS build directory."""

    build_dir: Path

    @property
    def root(self) -> Path:
        return self.build_dir / "package"

    @property
    def stage(self) -> Path:
        return self.root / "stage"

    @property
    def app(self) -> Path:
        return self.stage / f"{APP_NAME}.app"

    @property
    def contents(self) -> Path:
        return self.app / "Contents"

    @property
    def out(self) -> Path:
        return self.root / "out"

    @property
    def smoke(self) -> Path:
        return self.root / "smoke"

    @property
    def libraries(self) -> Path:
        """vcpkg's dylibs, which the app carries."""
        return macos.installed(self.build_dir) / macos.TRIPLET / "lib"

    @property
    def fake_core(self) -> Path:
        """The tests' fake libretro core, which the smoke run's runner draws."""
        return self.build_dir / "cmake" / "tests" / "retro" / "fake_core.dylib"

    def archive(self, version: str) -> Path:
        return self.out / f"{APP_NAME}-{version}-macos-{ARCH}.zip"
