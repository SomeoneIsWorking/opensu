"""Frame capture over VNC, for GL-backed displays where QMP screendump has no surface."""

import socket
import struct
import zlib
from pathlib import Path


def encode_png(width: int, height: int, rgb: bytes) -> bytes:
    """A truecolour PNG from packed RGB rows."""
    stride = width * 3
    raw = b"".join(b"\x00" + rgb[y * stride : (y + 1) * stride] for y in range(height))

    def chunk(kind: bytes, data: bytes) -> bytes:
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 6))
        + chunk(b"IEND", b"")
    )  # fmt: skip


def bgrx_to_rgb(pixels: bytes) -> bytes:
    """32-bit little-endian BGRX to packed RGB."""
    out = bytearray(len(pixels) // 4 * 3)
    out[0::3] = pixels[2::4]
    out[1::3] = pixels[1::4]
    out[2::3] = pixels[0::4]
    return bytes(out)


def _read(sock: socket.socket, count: int) -> bytes:
    data = bytearray()
    while len(data) < count:
        block = sock.recv(count - len(data))
        if not block:
            raise ConnectionError("VNC closed")
        data += block
    return bytes(data)


def capture(socket_path: Path, out: Path) -> Path:
    """One full frame from QEMU's VNC unix socket as a PNG."""
    sock = socket.socket(socket.AF_UNIX)
    sock.settimeout(30)
    sock.connect(str(socket_path))
    _read(sock, 12)
    sock.sendall(b"RFB 003.008\n")
    kinds = _read(sock, _read(sock, 1)[0])
    if 1 not in kinds:
        raise RuntimeError("the VNC server offers no 'none' security")
    sock.sendall(b"\x01")
    if _read(sock, 4) != b"\x00\x00\x00\x00":
        raise RuntimeError("VNC security handshake failed")
    sock.sendall(b"\x01")
    width, height = struct.unpack(">HH", _read(sock, 4))
    _read(sock, 16)
    _read(sock, struct.unpack(">I", _read(sock, 4))[0])
    # 32 bpp, depth 24, little endian, true colour, 8-bit channels at shifts 16, 8, 0.
    sock.sendall(
        b"\x00\x00\x00\x00"
        + struct.pack(">BBBBHHHBBB", 32, 24, 0, 1, 255, 255, 255, 16, 8, 0)
        + b"\x00\x00\x00"
    )
    sock.sendall(struct.pack(">BxHi", 2, 1, 0))
    sock.sendall(struct.pack(">BBHHHH", 3, 0, 0, 0, width, height))
    frame = bytearray(width * height * 4)
    covered = 0
    while covered < width * height:
        kind = _read(sock, 1)[0]
        if kind != 0:
            raise RuntimeError(f"unexpected VNC message {kind}")
        _read(sock, 1)
        rects = struct.unpack(">H", _read(sock, 2))[0]
        for _ in range(rects):
            x, y, w, h, encoding = struct.unpack(">HHHHi", _read(sock, 12))
            if encoding != 0:
                continue
            data = _read(sock, w * h * 4)
            for row in range(h):
                start = ((y + row) * width + x) * 4
                frame[start : start + w * 4] = data[row * w * 4 : (row + 1) * w * 4]
            covered += w * h
    sock.close()
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(encode_png(width, height, bgrx_to_rgb(bytes(frame))))
    return out
