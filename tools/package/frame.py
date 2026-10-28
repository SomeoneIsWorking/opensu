"""The frame a package's smoke run renders with `opensu --render`, checked by its size."""

import struct
from pathlib import Path

# What `--render` draws without OPENSU_WIDTH and OPENSU_HEIGHT.
RENDER_SIZE = (1280, 800)


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"{path} is not a PNG")
    return struct.unpack(">II", data[16:24])


def check(path: Path) -> None:
    """Refuses a missing frame or one that is not the shell's size."""
    if not path.is_file():
        raise RuntimeError(f"no frame was written to {path}")
    if png_size(path) != RENDER_SIZE:
        raise RuntimeError(f"the frame is {png_size(path)}, not {RENDER_SIZE}")
