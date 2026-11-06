"""The lab's network: an internet, two routers and two machines, each a network namespace.

Every namespace is held open by a `sleep`, and reached through its /proc entry, so nothing needs
`ip netns`'s root-owned directory.
"""

import os
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Literal

Nat = Literal["none", "full-cone", "restricted", "symmetric"]

STUN_ADDRESSES = ("192.0.2.1", "192.0.2.2")
STUN_PORT = 3478
HOST_PUBLIC = "203.0.113.2"
FRIEND_PUBLIC = "198.51.100.2"
HOST_PRIVATE = "10.0.1.2"
HOST_LAN_GATEWAY = "10.0.1.1"
FRIEND_PRIVATE = "10.0.2.2"
HOST_TAILNET = "100.64.0.1"
FRIEND_TAILNET = "100.64.0.2"


@dataclass
class Node:
    """A network namespace, held by the process `holder`."""

    name: str
    holder: subprocess.Popen[bytes]

    @property
    def prefix(self) -> list[str]:
        return ["nsenter", f"--net=/proc/{self.holder.pid}/ns/net", "--"]

    def run(self, *command: str) -> str:
        done = subprocess.run([*self.prefix, *command], check=True, capture_output=True, text=True)
        return done.stdout

    def start(self, command: list[str], log: Path) -> subprocess.Popen[bytes]:
        output = log.open("ab")
        return subprocess.Popen([*self.prefix, *command], stdout=output, stderr=subprocess.STDOUT)

    def ip(self, line: str) -> None:
        self.run("ip", *line.split())

    def forward(self) -> None:
        self.run("sh", "-c", "echo 1 > /proc/sys/net/ipv4/ip_forward")

    def nft(self, rules: str) -> None:
        subprocess.run([*self.prefix, "nft", "-f", "-"], check=True, input=rules, text=True)

    def close(self) -> None:
        self.holder.kill()
        self.holder.wait()


def node(name: str) -> Node:
    holder = subprocess.Popen(
        ["unshare", "--net", "sleep", "infinity"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    own = os.readlink("/proc/self/ns/net")
    for _ in range(200):
        if os.readlink(f"/proc/{holder.pid}/ns/net") != own:
            made = Node(name, holder)
            made.ip("link set lo up")
            return made
        time.sleep(0.01)
    holder.kill()
    holder.wait()
    raise RuntimeError(f"{name}: the namespace never appeared")


def wire(a: Node, a_name: str, a_ip: str, b: Node, b_name: str, b_ip: str) -> None:
    """A cable between two namespaces, each end with an address (`1.2.3.4/24`)."""
    a.ip(f"link add {a_name} type veth peer name {b_name} netns {b.holder.pid}")
    a.ip(f"addr add {a_ip} dev {a_name}")
    b.ip(f"addr add {b_ip} dev {b_name}")
    a.ip(f"link set {a_name} up")
    b.ip(f"link set {b_name} up")


def nat_rules(kind: Nat, private: str, public: str) -> str:
    """The nft ruleset of a router of `kind` for one machine at `private`."""
    if kind == "none":
        return ""
    if kind == "full-cone":
        post = f'oifname "wan0" ip saddr {private} snat to {public}'
        pre = f'iifname "wan0" ip daddr {public} dnat to {private}'
    elif kind == "restricted":
        post = 'oifname "wan0" masquerade'
        pre = ""
    else:
        post = 'oifname "wan0" masquerade fully-random'
        pre = ""
    return f"""
table ip nat {{
    chain pre {{
        type nat hook prerouting priority -100;
        {pre}
    }}
    chain post {{
        type nat hook postrouting priority 100;
        {post}
    }}
}}
"""


@dataclass
class Lab:
    internet: Node
    transit: Node
    host_router: Node
    friend_router: Node
    host: Node
    friend: Node

    def close(self) -> None:
        for each in (
            self.host,
            self.friend,
            self.host_router,
            self.friend_router,
            self.transit,
            self.internet,
        ):
            each.close()


def build(host_nat: Nat, friend_nat: Nat, tailnet: bool, delay_ms: int) -> Lab:
    internet = node("internet")
    transit = node("transit")
    host_router = node("host-router")
    friend_router = node("friend-router")
    host = node("host")
    friend = node("friend")
    for address in STUN_ADDRESSES:
        internet.ip(f"addr add {address}/32 dev lo")
    # A hop past the internet node, so a datagram held near its sender dies before the far router.
    wire(internet, "to-host", "172.16.0.1/30", transit, "up0", "172.16.0.2/30")
    wire(transit, "down0", "203.0.113.1/24", host_router, "wan0", f"{HOST_PUBLIC}/24")
    internet.ip("route add 203.0.113.0/24 via 172.16.0.2")
    transit.ip("route add default via 172.16.0.1")
    wire(internet, "to-friend", "198.51.100.1/24", friend_router, "wan0", f"{FRIEND_PUBLIC}/24")
    wire(host_router, "lan0", "10.0.1.1/24", host, "eth0", f"{HOST_PRIVATE}/24")
    wire(friend_router, "lan0", "10.0.2.1/24", friend, "eth0", f"{FRIEND_PRIVATE}/24")
    host_router.ip("route add default via 203.0.113.1")
    friend_router.ip("route add default via 198.51.100.1")
    host.ip("route add default via 10.0.1.1")
    friend.ip("route add default via 10.0.2.1")
    for each in (internet, transit, host_router, friend_router):
        each.forward()
    if host_nat != "none":
        host_router.nft(nat_rules(host_nat, HOST_PRIVATE, HOST_PUBLIC))
    if friend_nat != "none":
        friend_router.nft(nat_rules(friend_nat, FRIEND_PRIVATE, FRIEND_PUBLIC))
    if tailnet:
        wire(host, "ts0", f"{HOST_TAILNET}/30", friend, "ts0", f"{FRIEND_TAILNET}/30")
    if delay_ms:
        for link in ("to-host", "to-friend"):
            delay = f"{delay_ms}ms"
            internet.run("tc", "qdisc", "add", "dev", link, "root", "netem", "delay", delay)
    return Lab(internet, transit, host_router, friend_router, host, friend)
