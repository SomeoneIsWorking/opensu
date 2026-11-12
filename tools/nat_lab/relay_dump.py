"""Every event a Nostr relay holds, one JSON line each: a reader that only looks at the relay.

python -I relay_dump.py <host> <port>
"""

import base64
import json
import os
import socket
import struct
import sys


def frame(text: str) -> bytes:
    """One masked text frame, as a client sends it."""
    payload = text.encode()
    mask = os.urandom(4)
    size = len(payload)
    if size < 126:
        head = struct.pack("!BB", 0x81, 0x80 | size)
    elif size < 1 << 16:
        head = struct.pack("!BBH", 0x81, 0x80 | 126, size)
    else:
        head = struct.pack("!BBQ", 0x81, 0x80 | 127, size)
    return head + mask + bytes(byte ^ mask[at % 4] for at, byte in enumerate(payload))


def exactly(connection: socket.socket, count: int) -> bytes:
    data = b""
    while len(data) < count:
        chunk = connection.recv(count - len(data))
        if not chunk:
            raise RuntimeError("the relay closed the connection")
        data += chunk
    return data


def message(connection: socket.socket) -> str:
    first, second = exactly(connection, 2)
    size = second & 0x7F
    if size == 126:
        (size,) = struct.unpack("!H", exactly(connection, 2))
    elif size == 127:
        (size,) = struct.unpack("!Q", exactly(connection, 8))
    if second & 0x80:
        raise RuntimeError("the relay masked a frame")
    if first & 0x0F != 0x1:
        raise RuntimeError(f"not a text frame: {first:#x}")
    return exactly(connection, size).decode()


def main() -> None:
    host, port = sys.argv[1], int(sys.argv[2])
    connection = socket.create_connection((host, port), timeout=5)
    key = base64.b64encode(os.urandom(16)).decode()
    connection.sendall(
        (
            f"GET / HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
            f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        ).encode()
    )
    head = b""
    while b"\r\n\r\n" not in head:
        head += exactly(connection, 1)
    if not head.startswith(b"HTTP/1.1 101"):
        raise RuntimeError(f"no upgrade: {head[:40]!r}")
    connection.sendall(frame(json.dumps(["REQ", "dump", {"limit": 1000}])))
    while True:
        parts = json.loads(message(connection))
        if parts[0] == "EOSE":
            break
        if parts[0] == "EVENT":
            print(json.dumps(parts[2]))
    connection.close()


if __name__ == "__main__":
    main()
