"""A hidden openSU on this machine hosts Play online; openSU on an Android Virtual Device joins by
its code over ZeroTier alone, and each player's held A shows on both screens.

The desktop hosts because only a --hidden shell hands its code to tools; the device types it into
its own code card. Neither side has a STUN server, so they meet only through the lab's own ZeroTier
root on this machine's LAN address.
"""

import json
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

from tools.link_rom.build import build as build_rom
from tools.nat_lab import core_cache
from tools.nat_lab.android_device import AndroidDevice
from tools.nat_lab.frames import colour_at, pixel_rows
from tools.nat_lab.instance import Instance
from tools.nat_lab.memory_floor import MemoryFloor
from tools.nat_lab.topology import Host

ROOT_PORT = 39994
DEVICE_FORWARD = 7414
RUNNER_FORWARD = 7415
SHELL_PORT = 7311
NO_STUN = "127.0.0.1:9"
BLUE = 255
# The middle of the link ROM's two player bands, as a share of the runner's frame height.
BAND_SHARES = (100 / 720, 280 / 720)
AVD_MEMORY_MB = 2048
FLOOR_KIB = 1536 * 1024
APP_SECONDS = 240


@dataclass(frozen=True)
class Setup:
    out: Path
    build: Path
    repo: Path
    avd: str
    apk: Path | None


def lan_address() -> str:
    """This machine's address on the route out, which the device reaches it by."""
    words = subprocess.run(
        ["ip", "route", "get", "1.1.1.1"], check=True, capture_output=True, text=True
    ).stdout.split()
    return words[words.index("src") + 1]


def shot(instance: Instance, out: Path, name: str) -> Path:
    frame = out / "frames" / f"{instance.name}-{name}.png"
    instance.frame(frame)
    return frame


def has_core(device: AndroidDevice) -> bool:
    return f"{core_cache.CORE}.json" in device.as_app("ls -R files 2>/dev/null", check=False)


def download_core(device: AndroidDevice, shell: Instance) -> None:
    """mGBA (built-in) as a player gets it: Settings › Emulators › Emulators and cores."""
    shell.settings_page("Settings > Emulators")
    shell.press("a")
    shell.walk("emulators.page.downloads")
    shell.press("a")
    shell.walk(f"emulators.run.{core_cache.ENTRY}")
    shell.press("a")
    until = time.monotonic() + 300
    while not has_core(device):
        if time.monotonic() > until:
            raise RuntimeError("mGBA (built-in) never downloaded on the device")
        time.sleep(1)
    print("the device downloaded mGBA (built-in)", flush=True)
    for _ in range(6):
        shell.press("b")


def open_device(device: AndroidDevice, setup: Setup, roots: Path, rom: Path) -> Instance:
    """The app with the lab's settings, ROM and ZeroTier roots in its own files, answering."""
    device.stop_app()
    settings = setup.out / "device-settings.json"
    settings.write_text(
        json.dumps(
            {
                "onlineStunServers": NO_STUN,
                "onlineZeroTierRoots": f"{device.files}/zerotier-roots",
                "romFolders": [f"{device.files}/roms"],
            }
        )
    )
    device.push(settings, "files/settings.json")
    device.push(roots, "files/zerotier-roots")
    device.push(rom, "files/roms/gba/link_test.gba")
    device.forward(DEVICE_FORWARD, SHELL_PORT)
    device.adb("logcat", "-c")
    device.start_app()
    shell = Instance(
        "device",
        Host(),
        setup.out / "device",
        setup.build / "src" / "opensu",
        rom.parent.parent,
        NO_STUN,
    )
    shell.port = DEVICE_FORWARD
    until = time.monotonic() + APP_SECONDS
    while True:
        try:
            shell.state()
            break
        except (subprocess.CalledProcessError, json.JSONDecodeError):
            if time.monotonic() > until:
                raise
            if not device.app_running():
                # Right after a cold boot Android may refresh the package and end its activity.
                print("the app is not running: starting it again", flush=True)
                device.start_app()
            time.sleep(3)
    print("the device's control channel answers", flush=True)
    return shell


def host_code(desktop: Instance, out: Path) -> str:
    """The desktop hosts from the link ROM's page; its code, off its hidden channel."""
    desktop.ask_to_host()
    shot(desktop, out, "1-warning")
    desktop.press("a")
    state = desktop.wait_step("inviting", "failed", seconds=60)
    if state["onlineStep"] == "failed":
        raise RuntimeError("the desktop could not make a code")
    time.sleep(0.5)
    shot(desktop, out, "2-code")
    return state["onlineCode"]


def join(device: Instance, code: str, out: Path) -> None:
    """The device joins from Settings › Play online, the code typed into its card."""
    device.ask_to_join()
    shot(device, out, "1-warning")
    device.press("a")
    device.wait("at the code card", lambda state: state["screenFocus"] == "entry/field")
    device.type_text(code)
    shot(device, out, "2-code-entered")
    device.call("/key", "enter")


def bands(name: str, runners: tuple[Instance, Instance], out: Path) -> list[tuple[int, int, int]]:
    """Both players' bands on both screens, the frames kept; the two screens must agree."""
    seen = []
    for runner in runners:
        frame = shot(runner, out, name)
        height = len(pixel_rows(frame))
        width = int.from_bytes(frame.read_bytes()[16:20], "big")
        seen.append([colour_at(frame, width // 2, int(height * share)) for share in BAND_SHARES])
    if seen[0] != seen[1]:
        raise RuntimeError(f"{name}: the screens differ: {seen[0]} {seen[1]}")
    print(f"{name}: both screens show {seen[0]}", flush=True)
    return seen[0]


def play(device: AndroidDevice, shell: Instance, desktop: Instance, out: Path) -> None:
    """The device says yes; each player's held A shows on both screens. Pad B is the console's A,
    as pads name their buttons by place."""
    shell.wait("asked to play", lambda state: state["screen"] == "dialog", seconds=60)
    shot(shell, out, "4-question")
    shell.focus("dialog/1", "right", 3)
    shell.press("a")
    on_desktop = Instance(
        "desktop-runner", Host(), desktop.home, desktop.binary, desktop.roms, NO_STUN
    )
    on_desktop.port = desktop.runner_port()
    on_device = Instance("device-runner", Host(), shell.home, desktop.binary, desktop.roms, NO_STUN)
    device.forward(RUNNER_FORWARD, device.runner_port())
    on_device.port = RUNNER_FORWARD
    runners = (on_desktop, on_device)
    time.sleep(6)
    one, two = bands("5-game", runners, out)
    if BLUE in (one[2], two[2]):
        raise RuntimeError(f"a band is held before any press: {one} {two}")
    on_desktop.call("/input", '{"buttons":["b"]}')
    time.sleep(1)
    one, two = bands("6-desktop-holds-a", runners, out)
    if one[2] != BLUE or two[2] == BLUE:
        raise RuntimeError(f"the desktop's A did not cross: {one} {two}")
    on_desktop.call("/input", "{}")
    on_device.call("/input", '{"buttons":["b"]}')
    time.sleep(1)
    one, two = bands("7-device-holds-a", runners, out)
    if one[2] == BLUE or two[2] != BLUE:
        raise RuntimeError(f"the device's A did not cross: {one} {two}")
    on_device.call("/input", "{}")
    print("both players' A shown on both screens", flush=True)


def run(setup: Setup) -> None:
    setup.out.mkdir(parents=True, exist_ok=True)
    host = Host()
    roms = setup.out / "roms"
    rom = roms / "gba" / "link_test.gba"
    build_rom(rom, False)
    cores = core_cache.cores(setup.repo, setup.build, setup.repo / "scratch" / "nat-lab-cores")
    roots = setup.out / "zerotier-roots"
    root_log = setup.out / "zerotier-root.log"
    with MemoryFloor(FLOOR_KIB):
        root = host.start(
            [
                str(setup.build / "lucent" / "lucent_zerotier_root"),
                str(setup.out / "zerotier-root"),
                f"{lan_address()}/{ROOT_PORT}",
                str(roots),
            ],
            root_log,
        )
        device = None
        desktop = None
        try:
            until = time.monotonic() + 10
            while "root running" not in (root_log.read_text() if root_log.exists() else ""):
                if time.monotonic() > until:
                    raise RuntimeError(f"the root did not start: {root_log}")
                time.sleep(0.1)
            device = AndroidDevice(setup.avd, AVD_MEMORY_MB, setup.out / "emulator.log")
            if setup.apk is not None:
                device.install(setup.apk)
            shell = open_device(device, setup, roots, rom)
            if not has_core(device):
                download_core(device, shell)
            desktop = Instance(
                "desktop",
                host,
                setup.out / "desktop",
                setup.build / "src" / "opensu",
                roms,
                NO_STUN,
                {"onlineZeroTierRoots": str(roots)},
            )
            desktop.install_core(cores / f"{core_cache.CORE}.so")
            desktop.start()
            code = host_code(desktop, setup.out)
            join(shell, code, setup.out)
            play(device, shell, desktop, setup.out)
        finally:
            if device is not None:
                (setup.out / "device-logcat.txt").write_text(device.logcat())
            if desktop is not None:
                desktop.quit()
            if device is not None:
                device.stop()
            root.kill()
            root.wait()
