"""The remote-play shot: this openSU, in a headless Gamescope as a host must be, streams a game to
a handheld's openSU, and the handheld records its own screen.

The handheld runs openSU --hidden as a guest whose control channel listens on GUEST_PORT, reached
through an ssh tunnel, and has GUEST_RECORD: a script that records its screen for SECONDS to OUT.
The host is the player's own openSU run as usual, with its control channel on: a hidden run
works on copies of the player's files and would forget the pairing. Once paired, later runs find
the guest listing the host's games and skip pairing; until then the guest removes any stale half
it keeps and pairs, and the host accepts. The host encodes
H.264 through VA-API, so its environment needs a VA driver that has it (LIBVA_DRIVERS_PATH).
"""

import json
import os
import re
import subprocess
import time
import urllib.request
from collections.abc import Callable
from pathlib import Path

from tools.screenshots import scenes
from tools.screenshots.shell import HiddenShell, RealHome
from tools.trailer.shots import Shot

GUEST_PORT = 17398
TUNNEL_PORT = 17312
GUEST_RECORD = "$HOME/agents/opensu/record.sh"
GUEST_OUT = "$HOME/agents/opensu/run/Trailer/remote.mkv"
GAME = "/N64/Legend of Zelda, The - Ocarina of Time (USA).z64"
SECONDS = 9
SHOT = Shot("remote", "Stream games from your PC to a handheld", scenes._nothing, longest=6.0)


class _Host(HiddenShell):
    """The player's openSU, control channel on, inside the Gamescope openSU ships, which remote
    play captures."""

    def __init__(self, executable: Path, log: Path) -> None:
        super().__init__(executable, RealHome(), log)
        self._gamescope = executable.parent.parent / "libexec" / "opensu" / "gamescope"

    def __enter__(self) -> "_Host":
        env = dict(os.environ, WAYLAND_DISPLAY="", OPENSU_CONTROL_CHANNEL="1")
        self._log.parent.mkdir(parents=True, exist_ok=True)
        with self._log.open("wb") as log:
            self._process = subprocess.Popen(
                [str(self._gamescope), "--backend", "headless", "-W", "1920", "-H", "1080"]
                + ["-r", "60", "--", str(self._executable)],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        self._base = f"http://127.0.0.1:{self._wait_port(60)}"
        return self

    def peer_id(self) -> str:
        found = re.search(r"listening for other openSUs on port (\d+)", self._log.read_text())
        if found is None:
            raise RuntimeError(f"the host never listened for other openSUs: {self._log}")
        hello = f"http://127.0.0.1:{found.group(1)}/lan/hello"
        with urllib.request.urlopen(hello, timeout=10) as answer:
            return str(json.loads(answer.read())["id"])


def _guest() -> HiddenShell:
    guest = HiddenShell(Path("/"), RealHome(), Path("/"))
    guest._base = f"http://127.0.0.1:{TUNNEL_PORT}"
    return guest


def _walk(shell: HiddenShell, holds: Callable[[str], bool], what: str, way: str, limit: int = 20):
    for _ in range(limit):
        if holds(str(shell.state().get("screenFocus"))):
            return
        shell.press(way, settle=0.25)
    raise RuntimeError(f"never reached {what}: {shell.state()}")


def _to_home(guest: HiddenShell) -> None:
    """Back out of whatever the guest has open to its tiles."""
    for _ in range(8):
        state = guest.state()
        open_ = state.get("detailsOpen") or state.get("screen") or state.get("settingsOpen")
        if not open_ and not state.get("inGame") and not state.get("guideMenuOpen"):
            return
        guest.press("b", settle=0.6)
    raise RuntimeError(f"the guest never closed what it had open: {guest.state()}")


def _lists_games(guest: HiddenShell) -> bool:
    """Whether the guest, already paired, shows the host's games within a few looks."""
    try:
        guest.wait_until(lambda state: (state.get("games") or 0) > 0, "games", 25)
        return True
    except RuntimeError:
        return False


def _pair(guest: HiddenShell, host: _Host, host_id: str) -> None:
    """The guest's LAN page: Remove its stale half if it has one, then Pair; the host accepts."""
    # The Pair row shows only once the guest has found the host on the network.
    guest.wait_until(lambda state: (state.get("lanSeen") or 0) > 0, "the host found", 90)
    scenes._settings_page("LAN")(guest)
    guest.press("a")
    # The page's rows lead, the category tabs follow: walk down only as far as the pairing's row.
    forget, pair = "lan.forget." + host_id, "lan.pair." + host_id
    _walk(guest, lambda f: f in (forget, pair), "Pair or Remove", "down", 6)
    if guest.state().get("screenFocus") == forget:
        guest.press("a", settle=0.8)
        guest.wait_for("screen", "dialog")
        _walk(guest, lambda f: f == "dialog/1", "the Remove button", "right", 4)
        guest.press("a", settle=1.5)
        # Removed, the row is Pair in the same place.
        _walk(guest, lambda f: f == pair, "Pair", "down", 2)
    guest.press("a")
    host.wait_for("screen", "dialog", timeout=30)
    guest.wait_until(lambda state: bool(state.get("lanCode")), "a code", 30)
    _walk(host, lambda f: f == "dialog/1", "Accept", "right", 4)
    host.press("a")
    host.wait_until(lambda state: state.get("screen") != "dialog", "the dialog closed", 10)
    _to_home(guest)


def _moved(guest: HiddenShell, way: str) -> bool:
    before = guest.state().get("focusIndex")
    guest.press(way, settle=0.12)
    return guest.state().get("focusIndex") != before


def _to_game(guest: HiddenShell) -> None:
    """Along the host's shelf to GAME. The shelf runs in columns, down then across, and keeps its
    focus from the last visit: back to the first tile, then down and up each column in turn."""
    if str(guest.state().get("focusedId")).endswith(GAME):
        return
    for way in ("up", "left"):
        while _moved(guest, way):
            pass
    way = "down"
    for _ in range(200):
        while True:
            if str(guest.state().get("focusedId")).endswith(GAME):
                return
            if not _moved(guest, way):
                break
        way = "up" if way == "down" else "down"
        if not _moved(guest, "right"):
            break
    raise RuntimeError(f"no {GAME} on the host's shelf: {guest.state()}")


def _open_shelf(guest: HiddenShell) -> None:
    """Library, then the host's launcher tile, which leads it, above and left of the rest."""
    for _ in range(5):
        if guest.state().get("section") == "library":
            break
        guest.press("r1", settle=1.0)
    for _ in range(12):
        state = guest.state()
        if str(state.get("focusedId")).startswith("launcher:lan:"):
            break
        guest.press("left")
        if guest.state().get("focusIndex") == state.get("focusIndex"):
            guest.press("up")
    guest.press("a", settle=1.0)
    guest.wait_until(lambda state: state.get("shelf") not in ("library", "home"), "a shelf", 15)


def _play(guest: HiddenShell) -> None:
    """The host's shelf, unless the guest is still on it, along to GAME, its details, Play."""
    guest.wait_until(lambda state: (state.get("games") or 0) > 0, "the host's games", 30)
    if not str(guest.state().get("shelf")).startswith("launcher:lan:"):
        _open_shelf(guest)
    _to_game(guest)
    scenes._details(guest)
    _walk(guest, lambda f: f == "details.launch", "Play", "up")
    # A press while the page still opens goes nowhere: press again until remote play starts.
    for _ in range(3):
        time.sleep(0.8)
        guest.press("a", settle=0.05)
        try:
            guest.wait_until(lambda state: bool(state.get("remotePlay")), "remote play", 10)
            break
        except RuntimeError:
            continue
    guest.wait_until(lambda state: "decoded=" in str(state.get("remoteVideo", "")), "pictures", 90)


def _settle(guest: HiddenShell) -> None:
    """Once the host has quit, which ends the stream, the guest closes whatever it left open."""
    guest.wait_until(lambda state: not state.get("remotePlay"), "the stream ended", 30)
    _to_home(guest)


def record(opensu: Path, guest_host: str, work: Path, clips: Path, fmt) -> Path:
    """The remote-play clip in `clips`, recorded on `guest_host`'s screen."""
    tunnel = subprocess.Popen(
        ["ssh", "-N", "-o", "ExitOnForwardFailure=yes"]
        + ["-L", f"{TUNNEL_PORT}:127.0.0.1:{GUEST_PORT}", guest_host]
    )
    taken = work / "remote.mkv"
    try:
        guest = _guest()
        with _Host(opensu, work / "host.log") as host:
            host.wait_until(lambda state: (state.get("installed") or 0) > 0, "the library", 90)
            _to_home(guest)
            host_id = host.peer_id()
            for attempt in range(3):
                if _lists_games(guest):
                    break
                if attempt:
                    print(f"remote: pairing attempt {attempt} left the guest without games")
                _pair(guest, host, host_id)
            _play(guest)
            # The game's boot logos first; the title screen is what the shot shows.
            time.sleep(20)
            subprocess.run(["ssh", guest_host, f"{GUEST_RECORD} {SECONDS} {GUEST_OUT}"], check=True)
        _settle(guest)
    finally:
        tunnel.terminate()
    # scp names the guest's files from its home.
    fetched = f"{guest_host}:{GUEST_OUT.removeprefix('$HOME/')}"
    subprocess.run(["scp", "-q", fetched, str(taken)], check=True)
    clip = clips / f"{SHOT.name}.mp4"
    fit = (
        f"scale={fmt.width}:{fmt.height}:force_original_aspect_ratio=decrease:flags=lanczos,"
        f"pad={fmt.width}:{fmt.height}:(ow-iw)/2:(oh-ih)/2:color=black,setsar=1"
    )
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", str(taken), "-vf", f"{fit},fps={fmt.fps}"]
        + ["-an", "-c:v", "libx264", "-preset", "slow", "-crf", "14", str(clip)],
        check=True,
    )
    print(f"{SHOT.name}: {clip}")
    return clip
