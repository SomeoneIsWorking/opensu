"""First-boot configuration of the guest so it behaves like the user's machine."""

import time

from tools.vm import hostfiles
from tools.vm.config import TEST_USER
from tools.vm.guest import Guest

# Steam is not installed in the guest; the picker's game mode and openSU's Steam start run this.
STEAM_STUB = """#!/bin/sh
echo "$(date -Is) steam $*" >> "$HOME/steam-stub.log"
exec sleep infinity
"""

AUTOLOGIN_DROPIN = "/etc/sddm.conf.d/zz-vm-autologin.conf"


def apply_host_session_setup(guest: Guest) -> None:
    setup = hostfiles.read_host()
    guest.install_file(setup.picker, "/usr/bin/steamos-session-picker", "755")
    guest.install_file(setup.select, "/usr/bin/steamos-session-select", "755")
    guest.install_file(setup.picker_entry, "/usr/share/wayland-sessions/steam-picker.desktop")
    guest.install_file(setup.sddm_conf, "/etc/sddm.conf.d/kde_settings.conf")
    guest.install_file(STEAM_STUB.encode(), "/usr/local/bin/steam", "755")


def set_autologin(guest: Guest, session: str | None) -> None:
    """Override the host-like autologin: a session name, or None for a logged-out greeter."""
    if session is None:
        body = "[Autologin]\nUser=\nSession=\nRelogin=false\n"
    else:
        body = f"[Autologin]\nUser={TEST_USER}\nSession={session}\nRelogin=true\n"
    guest.install_file(body.encode(), AUTOLOGIN_DROPIN)


def clear_autologin_override(guest: Guest) -> None:
    guest.run(f"rm -f {AUTOLOGIN_DROPIN}", sudo=True)


def upgrade(guest: Guest) -> None:
    """The installer's release packages are older than the host's updates; match the host."""
    guest.run("dnf -y upgrade --refresh", sudo=True, timeout=3600)


def reboot(guest: Guest) -> None:
    """A real reboot: restarting SDDM leaves the previous session's user manager and compositor."""
    guest.run("systemctl --no-block reboot", sudo=True, check=False)
    deadline = time.monotonic() + 60
    while guest.run("true", timeout=10, check=False).code == 0 and time.monotonic() < deadline:
        time.sleep(2)
    guest.wait_ready()
