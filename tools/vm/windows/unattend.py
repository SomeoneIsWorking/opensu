"""The answer file and first-logon script of the unattended install, and the ISO carrying them.

Setup reads `autounattend.xml` from the root of any drive. The disk and network drivers come from
the virtio-win ISO in another drive, whose letter is not known ahead, so every likely letter is
listed. The first logon installs the virtio drivers and the QEMU guest agent, keeps the test user
logged on, and powers off, which ends the provisioning run.
"""

import subprocess
from pathlib import Path
from string import Template
from xml.sax.saxutils import escape

from tools.vm.windows.config import GUEST_DIR, TEST_PASSWORD, TEST_USER

DRIVE_LETTERS = "DEFGH"
# Each driver's folder on the virtio-win ISO; the disk is virtio-blk, the NIC virtio-net.
SETUP_DRIVERS = ("viostor", "NetKVM")
FIRST_LOGON = "first-logon.cmd"

_COMPONENT = (
    'processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" '
    'versionScope="nonSxS" xmlns:wcm="http://schemas.microsoft.com/WMIConfig/2002/State"'
)


def _driver_paths() -> str:
    paths = []
    key = 1
    for letter in DRIVE_LETTERS:
        for driver in SETUP_DRIVERS:
            paths.append(
                f'<PathAndCredentials wcm:action="add" wcm:keyValue="{key}">'
                f"<Path>{letter}:\\{driver}\\w11\\amd64</Path></PathAndCredentials>"
            )
            key += 1
    return "".join(paths)


def _first_logon_command() -> str:
    """Runs first-logon.cmd from whichever drive holds it."""
    letters = " ".join(DRIVE_LETTERS)
    return f"cmd /c for %d in ({letters}) do @if exist %d:\\{FIRST_LOGON} cmd /c %d:\\{FIRST_LOGON}"


def render_answer_file() -> str:
    template = Template((Path(__file__).parent / "autounattend.xml").read_text())
    return template.substitute(
        component=_COMPONENT,
        driver_paths=_driver_paths(),
        user=escape(TEST_USER),
        password=escape(TEST_PASSWORD),
        first_logon=escape(_first_logon_command()),
    )


def render_first_logon() -> str:
    """first-logon.cmd; it finds the virtio-win drive itself, which is not the answer file's."""
    user = TEST_USER
    password = TEST_PASSWORD
    winlogon = r"HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon"
    letters = " ".join(DRIVE_LETTERS)
    lines = [
        "@echo off",
        f"mkdir {GUEST_DIR}",
        f"set LOG={GUEST_DIR}\\first-logon.log",
        "set VIRTIO=",
        f"for %%d in ({letters}) do @if exist %%d:\\virtio-win-gt-x64.msi set VIRTIO=%%d:",
        "echo virtio-win on %VIRTIO% >> %LOG%",
        "msiexec /i %VIRTIO%\\virtio-win-gt-x64.msi /qn /norestart /l*v "
        f"{GUEST_DIR}\\virtio-msi.log",
        "echo virtio drivers %ERRORLEVEL% >> %LOG%",
        "msiexec /i %VIRTIO%\\guest-agent\\qemu-ga-x86_64.msi /qn /norestart /l*v "
        f"{GUEST_DIR}\\qemu-ga-msi.log",
        "echo guest agent %ERRORLEVEL% >> %LOG%",
        # Logged on at every boot, so the harness always has a desktop session to run in.
        f'reg add "{winlogon}" /v AutoAdminLogon /t REG_SZ /d 1 /f',
        f'reg add "{winlogon}" /v DefaultUserName /t REG_SZ /d {user} /f',
        f'reg add "{winlogon}" /v DefaultPassword /t REG_SZ /d {password} /f',
        f'reg delete "{winlogon}" /v AutoLogonCount /f',
        # No lock screen, display sleep or standby to hide the desktop from a run.
        "powercfg /change monitor-timeout-ac 0",
        "powercfg /change standby-timeout-ac 0",
        r'reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\Personalization" '
        "/v NoLockScreen /t REG_DWORD /d 1 /f",
        r'reg add "HKCU\Control Panel\Desktop" /v ScreenSaveActive /t REG_SZ /d 0 /f',
        "echo done >> %LOG%",
        "shutdown /s /t 10",
    ]
    return "\r\n".join(lines) + "\r\n"


def build_iso(work: Path) -> Path:
    """The answer-file ISO, rebuilt from the current templates."""
    content = work / "unattend"
    content.mkdir(parents=True, exist_ok=True)
    (content / "autounattend.xml").write_text(render_answer_file(), encoding="utf-8")
    (content / FIRST_LOGON).write_bytes(render_first_logon().encode("ascii"))
    iso = work / "unattend.iso"
    iso.unlink(missing_ok=True)
    subprocess.run(
        ["xorriso", "-as", "mkisofs", "-quiet", "-J", "-r", "-V", "OPENSU_UNATTEND",
         "-o", str(iso), str(content)],
        check=True,
    )  # fmt: skip
    return iso
