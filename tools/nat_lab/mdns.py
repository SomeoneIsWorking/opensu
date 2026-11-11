"""A machine's own system bus and Avahi inside its namespace, so LAN discovery runs as on a desktop.

Avahi runs as the lab's user in a mount namespace of its own: its runtime directory is a tmpfs, and
`/etc/passwd` and `/etc/group` name the user `avahi`, since Avahi checks who owns that directory.
"""

import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

from tools.nat_lab.topology import Node

BUS_POLICY = """<busconfig>
  <type>system</type>
  <listen>unix:path={socket}</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow user="*"/>
    <allow own="*"/>
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
  </policy>
</busconfig>
"""

AVAHI_CONFIG = """[server]
use-ipv4=yes
use-ipv6=no
enable-dbus=yes
ratelimit-interval-usec=0
[wide-area]
enable-wide-area=no
[publish]
publish-workstation=no
"""


@dataclass
class Mdns:
    """The bus and the daemon of one machine; `address` goes in `DBUS_SYSTEM_BUS_ADDRESS`."""

    address: str
    bus: subprocess.Popen[bytes]
    avahi: subprocess.Popen[bytes]

    def close(self) -> None:
        for each in (self.avahi, self.bus):
            each.kill()
            each.wait()


def start(at: Node, folder: Path, uid: int, gid: int) -> Mdns:
    folder.mkdir(parents=True, exist_ok=True)
    socket = folder / "bus"
    socket.unlink(missing_ok=True)
    (folder / "bus.conf").write_text(BUS_POLICY.format(socket=socket))
    (folder / "avahi.conf").write_text(AVAHI_CONFIG)
    (folder / "passwd").write_text(f"root:x:0:0::/:/bin/sh\navahi:x:{uid}:{gid}::/:/sbin/nologin\n")
    (folder / "group").write_text(f"root:x:0:\navahi:x:{gid}:\n")
    bus = at.start(
        ["dbus-daemon", f"--config-file={folder / 'bus.conf'}", "--nofork"], folder / "bus.log"
    )
    for _ in range(200):
        if socket.exists():
            break
        time.sleep(0.01)
    else:
        bus.kill()
        raise RuntimeError(f"{at.name}: the system bus never came up")
    address = f"unix:path={socket}"
    script = (
        "mount -t tmpfs tmpfs /run/avahi-daemon"
        f" && mount --bind {folder / 'passwd'} /etc/passwd"
        f" && mount --bind {folder / 'group'} /etc/group"
        " && exec avahi-daemon --no-drop-root --no-chroot --no-rlimits"
        f" -f {folder / 'avahi.conf'}"
    )
    avahi = at.start(
        ["env", f"DBUS_SYSTEM_BUS_ADDRESS={address}", "unshare", "--mount", "sh", "-c", script],
        folder / "avahi.log",
    )
    log = folder / "avahi.log"
    until = time.monotonic() + 10
    while "Server startup complete" not in log.read_text(errors="replace"):
        if avahi.poll() is not None or time.monotonic() > until:
            bus.kill()
            avahi.kill()
            raise RuntimeError(f"{at.name}: Avahi did not start: {log.read_text()}")
        time.sleep(0.05)
    return Mdns(address, bus, avahi)
