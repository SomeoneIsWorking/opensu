"""Small README images: lossy WebP through ImageMagick."""

import shutil
import subprocess
from pathlib import Path

QUALITY = 88


def require_magick() -> str:
    found = shutil.which("magick")
    if found is None:
        raise SystemExit(
            "ImageMagick is needed to encode WebP: `sudo dnf install ImageMagick` (Fedora), "
            "`sudo apt install imagemagick` (Debian/Ubuntu)"
        )
    return found


def to_webp(magick: str, png: Path, webp: Path) -> None:
    webp.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [magick, str(png), "-strip", "-quality", str(QUALITY), str(webp)],
        check=True,
    )
