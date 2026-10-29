"""openSU in the Windows guest: the CI installer installed as a player installs it, lavapipe as
the machine's Vulkan driver, the shell started on the user's desktop and driven through its
loopback control channel."""

import os
import time
from pathlib import Path

from tools.check import windows as check_windows
from tools.vm import test_rom
from tools.vm.windows import desktop
from tools.vm.windows.agent import Agent, AgentError, Finished
from tools.vm.windows.config import GUEST_DIR, WinConfig

CONTROL_URL = "http://127.0.0.1:7311"
# Under %LOCALAPPDATA%, where the per-user installer puts it.
INSTALLED = r"Programs\openSU\bin\opensu.exe"
LAVAPIPE_DIR = rf"{GUEST_DIR}\lavapipe"
LOG = rf"{GUEST_DIR}\opensu.log"
PIDFILE = rf"{GUEST_DIR}\opensu.pid"
ROMS = r"%USERPROFILE%\ROMs\NES"
ROM_NAME = "openSU Test Pattern.nes"
VULKAN_DRIVERS = r"HKLM\SOFTWARE\Khronos\Vulkan\Drivers"


def install(guest: Agent, installer: Path) -> None:
    """The installer run silently for the test user, as tools.package's smoke runs it."""
    remote = rf"{GUEST_DIR}\{installer.name}"
    guest.run("cmd.exe", ["/c", "mkdir", GUEST_DIR])
    guest.write_file(remote, installer.read_bytes())
    code = desktop.run(
        guest,
        "install",
        [f'"{remote}" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER '
         rf"/LOG={GUEST_DIR}\install.log"],
        timeout_s=900,
    )  # fmt: skip
    if code != 0:
        raise AgentError(f"the installer exited {code}; see {GUEST_DIR}\\install.log")


def install_lavapipe(cfg: WinConfig, guest: Agent) -> None:
    """Mesa's lavapipe from the pinned mesa-dist-win, registered for every process."""
    icd = check_windows.unpack_lavapipe(cfg.downloads / "lavapipe", dict(os.environ))
    guest.run("cmd.exe", ["/c", "mkdir", LAVAPIPE_DIR])
    for name in (icd.name, "vulkan_lvp.dll"):
        guest.write_file(rf"{LAVAPIPE_DIR}\{name}", (icd.parent / name).read_bytes())
    manifest = rf"{LAVAPIPE_DIR}\{icd.name}"
    done = guest.run(
        "reg.exe",
        ["add", VULKAN_DRIVERS, "/v", manifest, "/t", "REG_DWORD", "/d", "0", "/f"],
    )
    if done.code != 0:
        raise AgentError(f"could not register lavapipe: {done.out}{done.err}")


def place_test_rom(cfg: WinConfig, guest: Agent) -> str:
    """The generated test ROM in the test user's ROM folder; returns the folder."""
    local = test_rom.write(cfg.run / ROM_NAME)
    staged = rf"{GUEST_DIR}\{ROM_NAME}"
    guest.write_file(staged, local.read_bytes())
    code = desktop.run(
        guest, "rom", [f'mkdir "{ROMS}" 2>nul', f'copy /y "{staged}" "{ROMS}\\{ROM_NAME}"']
    )
    if code != 0:
        raise AgentError(f"could not place the test ROM ({code})")
    return ROMS


def start(guest: Agent, arguments: str = "") -> None:
    """openSU on the user's desktop, its log and its PID in the harness folder."""
    launch = (
        f"$p = Start-Process -FilePath (Join-Path $env:LOCALAPPDATA '{INSTALLED}') "
        f"{_argument_list(arguments)}-PassThru; "
        f"$p.Id | Out-File -Encoding ascii '{PIDFILE}'"
    )
    desktop.run(
        guest,
        "opensu",
        [f"set LUCENT_LOG_FILE={LOG}", f'powershell -NoProfile -Command "{launch}"'],
    )
    deadline = time.monotonic() + 120
    while not state(guest):
        if time.monotonic() > deadline:
            raise AgentError("openSU's control channel never answered")
        time.sleep(2)


def _argument_list(arguments: str) -> str:
    return f"-ArgumentList '{arguments}' " if arguments else ""


def stop(guest: Agent) -> None:
    """Ends the openSU `start` started, by its recorded PID, with what it launched."""
    pid = guest.read_file(PIDFILE).decode().strip()
    guest.run("taskkill.exe", ["/pid", pid, "/t", "/f"])


def _curl(guest: Agent, args: list[str]) -> Finished:
    return guest.run("curl.exe", ["-sf", *args], timeout_s=120)


def state(guest: Agent) -> str:
    return _curl(guest, [f"{CONTROL_URL}/state"]).out


def press(guest: Agent, button: str) -> str:
    """A pad button (`a`, `down`, `guide`, ...), as `tools.vm ui press` sends it."""
    done = _curl(guest, ["-d", button, f"{CONTROL_URL}/input"])
    if done.code != 0:
        raise AgentError(f"openSU refused the button {button!r}")
    return done.out


def frame(guest: Agent, local: Path) -> Path:
    """The shell's drawn frame as PNG."""
    remote = rf"{GUEST_DIR}\frame.png"
    if _curl(guest, ["-o", remote, f"{CONTROL_URL}/frame.png"]).code != 0:
        raise AgentError("openSU drew no frame")
    local.parent.mkdir(parents=True, exist_ok=True)
    local.write_bytes(guest.read_file(remote))
    guest.run("cmd.exe", ["/c", "del", remote])
    return local
