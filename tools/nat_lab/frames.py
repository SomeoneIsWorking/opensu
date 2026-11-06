"""Pixels of the runner's `/frame.png`, 8-bit RGB or RGBA, without an image library in the lock."""

import struct
import zlib
from pathlib import Path


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def pixel_rows(path: Path) -> list[bytes]:
    """Each row's bytes, filters undone."""
    data = path.read_bytes()
    at = 8
    idat = bytearray()
    width = 0
    channels = 0
    while at < len(data):
        (length,) = struct.unpack(">I", data[at : at + 4])
        kind = data[at + 4 : at + 8]
        body = data[at + 8 : at + 8 + length]
        at += 12 + length
        if kind == b"IHDR":
            width = struct.unpack(">I", body[:4])[0]
            if body[8] != 8 or body[9] not in (2, 6) or body[12] != 0:
                raise RuntimeError(f"{path}: not an 8-bit RGB or RGBA PNG")
            channels = 3 if body[9] == 2 else 4
        elif kind == b"IDAT":
            idat += body
    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    rows: list[bytes] = []
    previous = bytes(stride)
    for start in range(0, len(raw), stride + 1):
        kind = raw[start]
        line = bytearray(raw[start + 1 : start + 1 + stride])
        for x in range(stride):
            left = line[x - channels] if x >= channels else 0
            up = previous[x]
            corner = previous[x - channels] if x >= channels else 0
            if kind == 1:
                line[x] = (line[x] + left) & 255
            elif kind == 2:
                line[x] = (line[x] + up) & 255
            elif kind == 3:
                line[x] = (line[x] + (left + up) // 2) & 255
            elif kind == 4:
                line[x] = (line[x] + _paeth(left, up, corner)) & 255
        previous = bytes(line)
        rows.append(previous)
    return rows


def colour_at(path: Path, x: int, y: int) -> tuple[int, int, int]:
    rows = pixel_rows(path)
    channels = len(rows[0]) // (struct.unpack(">I", path.read_bytes()[16:20])[0])
    start = x * channels
    return (rows[y][start], rows[y][start + 1], rows[y][start + 2])
