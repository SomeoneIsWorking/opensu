"""Names and places of the Windows package."""

from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
APP_NAME = "openSU"
ARCH = "x86_64"
SCRIPT = REPO / "packaging" / "windows" / "opensu.iss"
# Inno Setup 6's compiler where its installer puts it, as on the windows-2025 runner image.
ISCC = Path(r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe")


@dataclass(frozen=True)
class Layout:
    """Everything the package writes, under the Windows build directory."""

    build_dir: Path

    @property
    def root(self) -> Path:
        return self.build_dir / "package"

    @property
    def stage(self) -> Path:
        return self.root / "stage"

    @property
    def out(self) -> Path:
        return self.root / "out"

    @property
    def smoke(self) -> Path:
        return self.root / "smoke"

    def installer(self, version: str) -> Path:
        return self.out / f"{APP_NAME}-{version}-{ARCH}-setup.exe"
