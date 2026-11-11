"""What the lab proves: each case one pair of networks and what must happen across them."""

import os
import re
import shutil
import signal
import subprocess
import sys
import time
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

from tools.link_rom.build import build as build_rom
from tools.nat_lab import mdns, topology
from tools.nat_lab.frames import colour_at
from tools.nat_lab.gb_rom import build as build_gb_rom
from tools.nat_lab.instance import Instance
from tools.nat_lab.topology import Lab, Nat


@dataclass
class Context:
    out: Path
    build: Path
    repo: Path


BLUE = 255
ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
# The middle of the link ROM's bands for players 1 and 2, in the runner's frame.
BAND_X = 640
BAND_ONE_Y = 100
BAND_TWO_Y = 280
# Inside a Game Boy's screen, and how dark a held A makes it.
SCREEN_Y = 400
DARK = 150
# Every integration off but LAN, which a case turns on.
LAN_ONLY = {name: False for name in ("steam", "epic", "gog", "emulators", "plugins")}


def invite_secret(code: str) -> str:
    """The 16 secret bytes of an invite, after its version and flags, in hex."""
    bits = "".join(format(ALPHABET.index(letter), "05b") for letter in code[3:])
    data = bytes(int(bits[at : at + 8], 2) for at in range(0, len(bits) - 7, 8))
    return data[2:18].hex()


def installed_core() -> Path:
    """The real mGBA core this machine's openSU downloaded: the link core for a GBA, and a core with
    no link for a Game Boy."""
    core = Path.home() / ".local" / "share" / "opensu" / "cores" / "mgba_libretro.so"
    if not core.exists():
        raise RuntimeError(f"{core} is missing: download mGBA (built-in) in openSU first")
    return core


def stun_list() -> str:
    return ",".join(f"{address}:{topology.STUN_PORT}" for address in topology.STUN_ADDRESSES)


class Rig:
    """A lab with a STUN server and a hidden openSU on each side."""

    def __init__(
        self,
        context: Context,
        host_nat: Nat,
        friend_nat: Nat,
        tailnet: bool = False,
        zerotier: bool = False,
        lan: bool = False,
        delay_ms: int = 20,
        gateway: str = "",
        lease: int = 120,
        any_core: bool = False,
    ) -> None:
        if context.out.exists():
            shutil.rmtree(context.out)
        context.out.mkdir(parents=True)
        self.context = context
        self.codes: list[str] = []
        self.lab: Lab = topology.build(host_nat, friend_nat, tailnet, zerotier, delay_ms)
        self.stun = self.lab.internet.start(
            [
                sys.executable,
                "-I",
                str(Path(__file__).with_name("stun_server.py")),
                str(topology.STUN_PORT),
                *topology.STUN_ADDRESSES,
            ],
            context.out / "stun.log",
        )
        self.capture = self.lab.internet.start(
            ["tcpdump", "-n", "-l", "-i", "any", f"udp and not port {topology.STUN_PORT}"],
            context.out / "wire.log",
        )
        self.zerotier_capture = (
            self.lab.host.start(
                ["tcpdump", "-n", "-l", "-i", "zt0", "udp or tcp"], context.out / "wire-zt0.log"
            )
            if zerotier
            else None
        )
        self.mdns: list[mdns.Mdns] = []
        if lan:
            for at in (self.lab.host, self.lab.friend):
                folder = context.out / f"{at.name}-mdns"
                self.mdns.append(mdns.start(at, folder, os.getuid(), os.getgid()))
        self.gateway: subprocess.Popen[bytes] | None = None
        self.gateway_log = context.out / "gateway.log"
        if gateway:
            self.gateway = self.lab.host_router.start(
                [
                    sys.executable,
                    "-I",
                    str(Path(__file__).with_name("gateway.py")),
                    gateway,
                    topology.HOST_LAN_GATEWAY,
                    topology.HOST_PUBLIC,
                    "--lease",
                    str(lease),
                ],
                self.gateway_log,
            )
            until = time.monotonic() + 5
            while "up" not in self.gateway_text() and time.monotonic() < until:
                time.sleep(0.05)
        roms = context.out / "roms"
        if any_core:
            build_gb_rom(roms / "gb" / "any_core.gb")
        else:
            (roms / "gba").mkdir(parents=True)
            build_rom(roms / "gba" / "link_test.gba", False)
        binary = context.build / "src" / "opensu"
        made = []
        for at, name in ((self.lab.host, "Host"), (self.lab.friend, "Friend")):
            settings = {"integrations": {**LAN_ONLY, "lan": lan}, "lanName": name} if lan else {}
            bus = self.mdns[len(made)].address if lan else ""
            folder = context.out / name.lower()
            made.append(
                Instance(name.lower(), at, folder, binary, roms, stun_list(), settings, bus)
            )
        self.host, self.friend = made
        for each in (self.host, self.friend):
            # mGBA's own release links GBA players and plays the Game Boy ROM alike.
            each.install_core(installed_core())
            each.start()

    def close(self) -> None:
        self.host.quit()
        self.friend.quit()
        for each in (self.stun, self.capture, self.zerotier_capture, self.gateway):
            if each is not None:
                each.kill()
                each.wait()
        for each in self.mdns:
            each.close()
        self.lab.close()

    def __enter__(self) -> "Rig":
        return self

    def __exit__(self, failed: object, *_: object) -> None:
        self.close()
        if failed is None:
            self.logs_keep_secrets()

    def shot(self, instance: Instance, name: str) -> None:
        instance.frame(self.frame(f"{instance.name}-{name}"))

    def frame(self, name: str) -> Path:
        return self.context.out / "frames" / f"{name}.png"

    def gateway_text(self) -> str:
        return self.gateway_log.read_text() if self.gateway_log.exists() else ""

    def wait_gateway(self, word: str, seconds: float) -> None:
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            if f" {word} " in self.gateway_text():
                return
            time.sleep(0.1)
        raise RuntimeError(f"the router never logged {word}: {self.gateway_text()}")

    def remember(self, code: str) -> None:
        self.codes.append(code)

    def logs_keep_secrets(self) -> None:
        """No log under the case's folder holds a code or the secret in one."""
        words = [*self.codes, *[invite_secret(code) for code in self.codes if code[:3] == "OSU"]]
        for log in self.context.out.rglob("*.log"):
            text = log.read_text(errors="replace").lower()
            for word in words:
                if word.lower() in text:
                    raise RuntimeError(f"{log} holds a secret")
        print(f"{len(self.codes)} codes in no log")


def host_offers(rig: Rig) -> str:
    """Play online on the link ROM's page: the warning read, the code made and returned."""
    host = rig.host
    host.wait("in the library", lambda state: state["games"] > 0)
    host.press("a", "a")
    host.wait("on the details page", lambda state: state["detailsOpen"])
    host.focus("details.playOnline")
    host.press("a")
    host.wait("asked first", lambda state: state["screen"] == "dialog")
    rig.shot(host, "1-warning")
    host.press("a")
    state = host.wait_step("inviting", "failed", seconds=15)
    if state["onlineStep"] == "failed":
        raise RuntimeError("the host could not make a code")
    time.sleep(0.5)
    rig.shot(host, "2-code")
    code = state["onlineCode"]
    rig.remember(code)
    print(f"code is {len(code)} characters")
    return code


def friend_enters(rig: Rig, code: str) -> None:
    """Settings › Play online › Join, the warning read, the code entered."""
    friend = rig.friend
    friend.wait("up", lambda state: state["screen"] == "" and state["games"] >= 0)
    friend.press("guide")
    friend.wait("in the Guide menu", lambda state: state["guideMenuOpen"])
    friend.focus("guide.settings")
    friend.press("a")
    friend.wait("in Settings", lambda state: state["settingsOpen"])
    friend.focus("categories/online")
    friend.press("a")
    friend.focus("online.join")
    friend.press("a")
    friend.wait("asked first", lambda state: state["screen"] == "dialog")
    rig.shot(friend, "1-warning")
    friend.press("a")
    friend.wait("at the code card", lambda state: state["screenFocus"] == "entry/field")
    friend.type_text(code)
    rig.shot(friend, "2-code-entered")
    friend.call("/key", "enter")


def reply_back(rig: Rig) -> None:
    """The friend's reply code, pasted into the host's field as a chat app's copy would be."""
    friend = rig.friend
    state = friend.wait_step("replying", "failed", seconds=20)
    if state["onlineStep"] == "failed":
        raise RuntimeError("the friend failed before a reply")
    time.sleep(0.5)
    rig.shot(friend, "3-reply-code")
    code = state["onlineCode"]
    rig.remember(code)
    print(f"reply is {len(code)} characters")
    host = rig.host
    host.focus("online/reply")
    host.press("a")
    host.call("/clipboard", f" {code}\n")
    host.call("/key", "ctrl+v")
    time.sleep(0.3)
    rig.shot(host, "3-reply-pasted")
    host.call("/key", "enter")


def bands(rig: Rig, name: str, host_port: int, friend_port: int) -> list[tuple[int, int, int]]:
    """Both bands of both screens, the frames kept; the two screens must agree."""
    host_frame = rig.frame(f"host-{name}")
    friend_frame = rig.frame(f"friend-{name}")
    rig.host.frame(host_frame, host_port)
    rig.friend.frame(friend_frame, friend_port)
    host_bands = [colour_at(host_frame, BAND_X, y) for y in (BAND_ONE_Y, BAND_TWO_Y)]
    friend_bands = [colour_at(friend_frame, BAND_X, y) for y in (BAND_ONE_Y, BAND_TWO_Y)]
    if host_bands != friend_bands:
        raise RuntimeError(f"{name}: the screens differ: {host_bands} {friend_bands}")
    return host_bands


def holding_blue(band: tuple[int, int, int]) -> bool:
    return band[2] == BLUE


def play(rig: Rig) -> tuple[int, int]:
    """The friend says yes, and each player's held A shows on both screens. Pad B is the console's
    A, as pads name their buttons by place. Returns the two runners' control ports."""
    friend = rig.friend
    friend.wait("asked to play", lambda state: state["screen"] == "dialog", seconds=20)
    rig.shot(friend, "4-question")
    friend.focus("dialog/1", "right", 3)
    friend.press("a")
    host_port = rig.host.runner_port()
    friend_port = rig.friend.runner_port()
    time.sleep(6)
    one, two = bands(rig, "5-game", host_port, friend_port)
    if holding_blue(one) or holding_blue(two) or one[0] == 0 or two[1] == 0:
        raise RuntimeError(f"the bands are not red and green: {one} {two}")
    rig.host.call("/input", '{"buttons":["b"]}', host_port)
    time.sleep(1)
    one, two = bands(rig, "6-host-holds-a", host_port, friend_port)
    if not holding_blue(one) or holding_blue(two):
        raise RuntimeError(f"the host's A did not cross: {one} {two}")
    rig.host.call("/input", "{}", host_port)
    rig.friend.call("/input", '{"buttons":["b"]}', friend_port)
    time.sleep(1)
    one, two = bands(rig, "7-friend-holds-a", host_port, friend_port)
    if holding_blue(one) or not holding_blue(two):
        raise RuntimeError(f"the friend's A did not cross: {one} {two}")
    rig.friend.call("/input", "{}", friend_port)
    print("both players' A shown on both screens")
    return host_port, friend_port


def ends_cleanly(rig: Rig, stop: signal.Signals) -> None:
    """The host's runner gets `stop`; both shells leave the game and hold no socket of it."""
    rig.host.signal_runner(stop)
    for each in (rig.host, rig.friend):
        each.wait("out of the game", lambda state: not state["inGame"], seconds=40)
    time.sleep(1)
    rig.shot(rig.host, "8-after")
    rig.shot(rig.friend, "8-after")
    for each in (rig.host, rig.friend):
        left = each.udp_ports() - each.ports_before
        if left:
            raise RuntimeError(f"{each.name} still holds {left}")
    print("no port held after the session")


def plain_failure(rig: Rig, who: Instance, started: float, most: float, name: str) -> None:
    state = who.wait_step("failed", seconds=most + 5)
    took = time.monotonic() - started
    time.sleep(0.5)
    rig.shot(who, name)
    print(f"{who.name} failed after {took:.1f} s at {state['onlineStep']}")
    if took > most:
        raise RuntimeError(f"{who.name} took {took:.1f} s, over {most} s")


def dark(path: Path) -> bool:
    return sum(colour_at(path, BAND_X, SCREEN_Y)) < DARK


def shows_held(rig: Rig, name: str, host_port: int, friend_port: int) -> bool:
    """Whether both screens are black; they must agree."""
    host_frame = rig.frame(f"host-{name}")
    friend_frame = rig.frame(f"friend-{name}")
    rig.host.frame(host_frame, host_port)
    rig.friend.frame(friend_frame, friend_port)
    if dark(host_frame) != dark(friend_frame):
        raise RuntimeError(f"{name}: the screens differ")
    return dark(host_frame)


def session_line(who: Instance) -> str:
    lines = [line for line in who.log.read_text().splitlines() if "netplay: " in line]
    return "\n".join(lines)


def any_core(context: Context) -> None:
    """A real non-link core: one console on each machine, rollback under 120 ms of round trip."""
    with Rig(context, "restricted", "restricted", delay_ms=60, any_core=True) as rig:
        code = host_offers(rig)
        friend_enters(rig, code)
        reply_back(rig)
        friend = rig.friend
        friend.wait("asked to play", lambda state: state["screen"] == "dialog", seconds=20)
        rig.shot(friend, "4-question")
        friend.focus("dialog/1", "right", 3)
        friend.press("a")
        host_port = rig.host.runner_port()
        friend_port = rig.friend.runner_port()
        time.sleep(6)
        if shows_held(rig, "5-game", host_port, friend_port):
            raise RuntimeError("the screens start black")
        for _ in range(12):
            rig.host.call("/input", '{"buttons":["b"]}', host_port)
            rig.friend.call("/input", '{"buttons":["b"]}', friend_port)
            time.sleep(0.25)
            rig.host.call("/input", "{}", host_port)
            rig.friend.call("/input", "{}", friend_port)
            time.sleep(0.25)
        rig.host.call("/input", '{"buttons":["b"]}', host_port)
        time.sleep(1)
        if not shows_held(rig, "6-host-holds-a", host_port, friend_port):
            raise RuntimeError("the host's A did not reach both consoles")
        rig.host.call("/input", "{}", host_port)
        time.sleep(1)
        if shows_held(rig, "7-let-go", host_port, friend_port):
            raise RuntimeError("letting go did not reach both consoles")
        time.sleep(3)
        ends_cleanly(rig, signal.SIGTERM)
        run_again = 0
        for each in (rig.host, rig.friend):
            print(f"{each.name}:\n{session_line(each)}")
            ran = re.search(
                r"(\d+) frames run again, (\d+) state checks matched", session_line(each)
            )
            if ran is None or int(ran.group(2)) == 0:
                raise RuntimeError(f"{each.name}: no state check matched")
            if "rollback up to" not in session_line(each):
                raise RuntimeError(f"{each.name} did not roll back")
            run_again += int(ran.group(1))
        # The machine running ahead of the other is the one that guesses.
        if run_again == 0:
            raise RuntimeError("no frame was run again")


def full_cone(context: Context) -> None:
    with Rig(context, "full-cone", "restricted") as rig:
        code = host_offers(rig)
        friend_enters(rig, code)
        play(rig)
        ends_cleanly(rig, signal.SIGTERM)


def restricted(context: Context) -> None:
    with Rig(context, "restricted", "restricted") as rig:
        code = host_offers(rig)
        friend_enters(rig, code)
        reply_back(rig)
        play(rig)
        ends_cleanly(rig, signal.SIGTERM)


def symmetric(context: Context) -> None:
    with Rig(context, "symmetric", "symmetric") as rig:
        code = host_offers(rig)
        friend_enters(rig, code)
        plain_failure(rig, rig.friend, time.monotonic(), 8.0, "3-failed")
        rig.shot(rig.host, "3-still-offering")


def symmetric_against_restricted(context: Context) -> None:
    with Rig(context, "symmetric", "restricted") as rig:
        code = host_offers(rig)
        friend_enters(rig, code)
        started = time.monotonic()
        reply_back(rig)
        plain_failure(rig, rig.host, time.monotonic(), 20.0, "4-failed")
        plain_failure(rig, rig.friend, started, 130.0, "4-failed")


def tailscale(context: Context) -> None:
    with Rig(context, "symmetric", "symmetric", tailnet=True) as rig:
        code = host_offers(rig)
        friend_enters(rig, code)
        play(rig)
        ends_cleanly(rig, signal.SIGTERM)


def zerotier(context: Context) -> None:
    """Symmetric NATs, and a ZeroTier network the only way between; idle bridges on the host."""
    with Rig(context, "symmetric", "symmetric", zerotier=True) as rig:
        code = host_offers(rig)
        print(gathered_line(rig.host))
        friend_enters(rig, code)
        play(rig)
        ends_cleanly(rig, signal.SIGTERM)
        print(zerotier_traffic(rig))


def zerotier_lan(context: Context) -> None:
    """LAN pairing where the machines share only a ZeroTier network, which is no default route."""
    with Rig(context, "restricted", "restricted", zerotier=True, lan=True) as rig:
        for each in (rig.host, rig.friend):
            each.wait("seeing the other", lambda state: state["lanSeen"] >= 1, seconds=30)
            print(found_line(each))
        lan_pair(rig)
        print(zerotier_traffic(rig))


def gathered_line(who: Instance) -> str:
    lines = [line for line in who.log.read_text().splitlines() if "public addresses" in line]
    return lines[-1] if lines else f"{who.name}: no gathered line"


def found_line(who: Instance) -> str:
    lines = [line for line in who.log.read_text().splitlines() if "[lan] found" in line]
    return lines[-1] if lines else f"{who.name}: found nobody"


def zerotier_traffic(rig: Rig) -> str:
    log = rig.context.out / "wire-zt0.log"
    lines = log.read_text().splitlines() if log.exists() else []
    lines = [line for line in lines if not line.startswith(("tcpdump:", "listening on"))]
    if not lines:
        raise RuntimeError("nothing crossed zt0")
    return f"{len(lines)} packets on the host's zt0, first: {lines[0]}"


def lan_pair(rig: Rig) -> None:
    """Settings › LAN › Pair on the friend, Accept on the host, both paired."""
    friend, host = rig.friend, rig.host
    friend.press("guide")
    friend.wait("in the Guide menu", lambda state: state["guideMenuOpen"])
    friend.focus("guide.settings")
    friend.press("a")
    friend.wait("in Settings", lambda state: state["settingsOpen"])
    friend.focus("categories/lan")
    friend.press("a")
    for _ in range(30):
        if str(friend.state()["screenFocus"]).startswith("lan.pair."):
            break
        friend.press("down")
    else:
        raise RuntimeError(f"no Pair row: {friend.state()['screenFocus']}")
    rig.shot(friend, "1-pair-row")
    friend.press("a")
    host.wait("asked to pair", lambda state: state["screen"] == "dialog", seconds=30)
    friend.wait("showing a code", lambda state: bool(state["lanCode"]), seconds=30)
    rig.shot(friend, "2-pair-code")
    rig.shot(host, "2-pair-question")
    host.focus("dialog/1", "right", 4)
    host.press("a")
    for each in (friend, host):
        each.wait("paired", lambda state: state["lanPaired"] == 1, seconds=30)
    time.sleep(0.5)
    rig.shot(friend, "3-paired")
    rig.shot(host, "3-paired")
    print("paired over zt0")


def nat_pmp(context: Context) -> None:
    """Short leases: renewed in play, removed after a dead runner, lapsing after a dead shell."""
    with Rig(context, "restricted", "restricted", gateway="nat-pmp", lease=16) as rig:
        code = host_offers(rig)
        rig.wait_gateway("add", 1)
        friend_enters(rig, code)
        play(rig)
        rig.wait_gateway("renew", 20)
        ends_cleanly(rig, signal.SIGKILL)
        rig.wait_gateway("remove", 5)
        print("mapping removed after a killed runner")
        rig.host.press("a")
        rig.host.wait("back on the details page", lambda state: state["detailsOpen"])
        rig.host.focus("details.playOnline")
        rig.host.press("a")
        rig.host.wait("asked first", lambda state: state["screen"] == "dialog")
        rig.host.press("a")
        rig.host.wait_step("inviting", seconds=15)
        rig.host.kill()
        rig.wait_gateway("lapse", 20)
        print("mapping lapsed after a killed shell")


def upnp(context: Context) -> None:
    with Rig(context, "restricted", "restricted", gateway="upnp") as rig:
        code = host_offers(rig)
        rig.wait_gateway("add", 1)
        friend_enters(rig, code)
        play(rig)
        ends_cleanly(rig, signal.SIGTERM)
        rig.wait_gateway("remove", 5)
        print("mapping removed at the end of the session")


CASES: dict[str, Callable[[Context], None]] = {
    "full-cone": full_cone,
    "restricted": restricted,
    "symmetric": symmetric,
    "symmetric-against-restricted": symmetric_against_restricted,
    "tailscale": tailscale,
    "zerotier": zerotier,
    "zerotier-lan": zerotier_lan,
    "nat-pmp": nat_pmp,
    "upnp": upnp,
    "any-core": any_core,
}
