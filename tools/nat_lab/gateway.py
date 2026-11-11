"""A home router's port mapping: NAT-PMP or UPnP on its LAN side, forwarding through nft and
logging each mapping's life, which is what the lab reads to prove one went away.

Run inside the router's namespace with `python -I`, so standard library only.
"""

import argparse
import http.server
import re
import socket
import struct
import subprocess
import sys
import threading
import time
from dataclasses import dataclass

NAT_PMP_PORT = 5351
SSDP_GROUP = "239.255.255.250"
SSDP_PORT = 1900
HTTP_PORT = 5000
SERVICE = "urn:schemas-upnp-org:service:WANIPConnection:1"
DESCRIPTION = f"""<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0"><device>
<deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
<serviceList><service><serviceType>{SERVICE}</serviceType>
<controlURL>/control</controlURL></service></serviceList></device></root>
"""


def say(line: str) -> None:
    print(f"{time.monotonic():.1f} {line}", flush=True)


@dataclass
class Forward:
    client: str
    port: int
    until: float


class Forwards:
    """The mappings in force, which are the rules of one nft chain."""

    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.held: dict[int, Forward] = {}
        # nft 1.1 wants a separator after a chain's closing brace.
        self.nft("table ip gateway { chain pre { type nat hook prerouting priority -110; }; }")

    @staticmethod
    def nft(rules: str) -> None:
        subprocess.run(["nft", "-f", "-"], check=True, input=rules, text=True)

    def apply(self) -> None:
        lines = ["flush chain ip gateway pre"]
        for external, forward in self.held.items():
            lines.append(
                f'add rule ip gateway pre iifname "wan0" udp dport {external} '
                f"dnat to {forward.client}:{forward.port}"
            )
        self.nft("\n".join(lines))

    def add(self, external: int, client: str, port: int, lease: int) -> None:
        with self.lock:
            known = external in self.held
            self.held[external] = Forward(client, port, time.monotonic() + lease)
            if not known:
                self.apply()
            say(f"{'renew' if known else 'add'} {external} to {client}:{port} for {lease}s")

    def remove(self, external: int) -> None:
        with self.lock:
            if self.held.pop(external, None):
                self.apply()
                say(f"remove {external}")

    def remove_port(self, client: str, port: int) -> None:
        for external, forward in list(self.held.items()):
            if forward.client == client and forward.port == port:
                self.remove(external)

    def lapse(self) -> None:
        while True:
            time.sleep(0.25)
            with self.lock:
                gone = [e for e, f in self.held.items() if f.until < time.monotonic()]
                for external in gone:
                    del self.held[external]
                    say(f"lapse {external}")
                if gone:
                    self.apply()


def nat_pmp(lan: str, public: str, lease: int, forwards: Forwards) -> None:
    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.bind((lan, NAT_PMP_PORT))
    while True:
        request, sender = server.recvfrom(64)
        epoch = int(time.monotonic()) & 0xFFFFFFFF
        if len(request) == 2 and request[1] == 0:
            server.sendto(struct.pack("!BBHI", 0, 128, 0, epoch) + socket.inet_aton(public), sender)
        elif len(request) == 12 and request[1] == 1:
            internal, external, wanted = struct.unpack("!HHI", request[4:12])
            if wanted == 0:
                forwards.remove_port(sender[0], internal)
                granted, external = 0, 0
            else:
                granted = min(wanted, lease)
                external = external or internal
                forwards.add(external, sender[0], internal, granted)
            server.sendto(
                struct.pack("!BBHIHHI", 0, 129, 0, epoch, internal, external, granted), sender
            )


def ssdp(lan: str) -> None:
    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("", SSDP_PORT))
    membership = socket.inet_aton(SSDP_GROUP) + socket.inet_aton(lan)
    server.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
    answer = (
        "HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\n"
        f"LOCATION: http://{lan}:{HTTP_PORT}/description.xml\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n"
    ).encode()
    while True:
        request, sender = server.recvfrom(2048)
        if request.startswith(b"M-SEARCH"):
            server.sendto(answer, sender)


def field(body: str, name: str) -> str:
    found = re.search(f"<{name}>([^<]*)</{name}>", body)
    return found.group(1) if found else ""


def upnp_handler(public: str, forwards: Forwards) -> type[http.server.BaseHTTPRequestHandler]:
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, format: str, *args: object) -> None:
            pass

        def answer(self, text: str) -> None:
            body = text.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/xml")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:
            self.answer(DESCRIPTION)

        def do_POST(self) -> None:
            body = self.rfile.read(int(self.headers["Content-Length"])).decode()
            action = self.headers["SOAPAction"].strip('"').split("#")[1]
            inner = ""
            if action == "GetExternalIPAddress":
                inner = f"<NewExternalIPAddress>{public}</NewExternalIPAddress>"
            elif action == "AddPortMapping":
                forwards.add(
                    int(field(body, "NewExternalPort")),
                    field(body, "NewInternalClient"),
                    int(field(body, "NewInternalPort")),
                    int(field(body, "NewLeaseDuration")),
                )
            elif action == "DeletePortMapping":
                forwards.remove(int(field(body, "NewExternalPort")))
            self.answer(
                '<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/'
                f'envelope/"><s:Body><u:{action}Response xmlns:u="{SERVICE}">{inner}'
                f"</u:{action}Response></s:Body></s:Envelope>"
            )

    return Handler


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=["nat-pmp", "upnp"])
    parser.add_argument("lan")
    parser.add_argument("public")
    parser.add_argument("--lease", type=int, default=120)
    args = parser.parse_args()
    forwards = Forwards()
    threading.Thread(target=forwards.lapse, daemon=True).start()
    say("up")
    if args.kind == "nat-pmp":
        nat_pmp(args.lan, args.public, args.lease, forwards)
    else:
        threading.Thread(target=ssdp, args=(args.lan,), daemon=True).start()
        server = http.server.ThreadingHTTPServer(
            (args.lan, HTTP_PORT), upnp_handler(args.public, forwards)
        )
        server.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
