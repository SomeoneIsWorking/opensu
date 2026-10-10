"""Where the VM lives and the fixed facts about the guest."""

import os
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FEDORA_RELEASE = "44"
TREE_URL = (
    f"https://dl.fedoraproject.org/pub/fedora/linux/releases/{FEDORA_RELEASE}/Everything/x86_64/os"
)
MIRRORLIST = "https://mirrors.fedoraproject.org/mirrorlist"

TEST_USER = "tester"
# Throwaway credential of the disposable guest; it protects nothing.
TEST_PASSWORD = "opensu-test"

# Host files the guest reproduces (read only).
HOST_PICKER = Path("/usr/bin/steamos-session-picker")
HOST_SELECT = Path("/usr/bin/steamos-session-select")
HOST_PICKER_ENTRY = Path("/usr/share/wayland-sessions/steam-picker.desktop")
HOST_SDDM_CONF = Path("/etc/sddm.conf.d/kde_settings.conf")


@dataclass(frozen=True)
class VmConfig:
    root: Path
    shots: Path
    ssh_port: int = 22022
    memory_mb: int = 4096
    cpus: int = 4
    disk_gb: int = 32
    width: int = 1280
    height: int = 800

    @property
    def downloads(self) -> Path:
        return self.root / "downloads"

    @property
    def base_image(self) -> Path:
        return self.root / "base.qcow2"

    @property
    def run(self) -> Path:
        return self.root / "run"

    @property
    def overlay(self) -> Path:
        return self.run / "overlay.qcow2"

    @property
    def qmp_socket(self) -> Path:
        return self.run / "qmp.sock"

    @property
    def vnc_socket(self) -> Path:
        return self.run / "vnc.sock"

    @property
    def pidfile(self) -> Path:
        return self.run / "qemu.pid"

    @property
    def qemu_log(self) -> Path:
        return self.run / "qemu.log"

    @property
    def serial_log(self) -> Path:
        return self.run / "serial.log"

    @property
    def ssh_key(self) -> Path:
        return self.root / "id_ed25519"

    @property
    def known_hosts(self) -> Path:
        return self.run / "known_hosts"

    def snapshot(self, name: str) -> Path:
        return self.root / f"snap-{name}.qcow2"


def load() -> VmConfig:
    root = Path(os.environ.get("OPENSU_VM_DIR", "/mnt/Boy/vm/opensu"))
    shots = Path(os.environ.get("OPENSU_VM_SHOTS", str(REPO / "scratch" / "vm" / "shots")))
    return VmConfig(root=root, shots=shots)
