"""Command line: `uv run --frozen python -m tools.package <platform> ...`.

appimage <command> [--version V]   the Linux AppImage (tools/package/appimage)
windows [--version V] [--build-dir D]   the Windows installer (tools/package/windows)
"""

import sys

from tools.package.appimage import cli as appimage
from tools.package.windows import cli as windows

PLATFORMS = {"appimage": appimage.main, "windows": windows.main}


def main(argv: list[str] | None = None) -> int:
    words = sys.argv[1:] if argv is None else argv
    if not words or words[0] not in PLATFORMS:
        print(__doc__, file=sys.stderr)
        return 2
    return PLATFORMS[words[0]](words[1:])
