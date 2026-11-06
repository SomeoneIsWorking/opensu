"""A STUN server for the lab's internet: answers Binding requests on each address it is given."""

import selectors
import socket
import struct
import sys

COOKIE = 0x2112A442


def answer(request: bytes, address: tuple[str, int]) -> bytes | None:
    if len(request) < 20 or request[0:2] != b"\x00\x01":
        return None
    ip = struct.unpack("!I", socket.inet_aton(address[0]))[0] ^ COOKIE
    port = address[1] ^ (COOKIE >> 16)
    attribute = struct.pack("!HHBBHI", 0x0020, 8, 0, 1, port, ip)
    return b"\x01\x01" + struct.pack("!H", len(attribute)) + request[4:20] + attribute


def main() -> None:
    port = int(sys.argv[1])
    selector = selectors.DefaultSelector()
    for address in sys.argv[2:]:
        bound = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        bound.bind((address, port))
        selector.register(bound, selectors.EVENT_READ)
    while True:
        for key, _ in selector.select():
            server = key.fileobj
            assert isinstance(server, socket.socket)
            request, sender = server.recvfrom(2048)
            reply = answer(request, sender)
            if reply is not None:
                server.sendto(reply, sender)


if __name__ == "__main__":
    main()
