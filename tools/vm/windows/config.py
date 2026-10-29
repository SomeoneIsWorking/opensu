"""Where the Windows guest lives, the pinned media it installs from and the fixed facts about it."""

import os
from dataclasses import dataclass
from pathlib import Path

from tools.vm.config import REPO

# Windows 11 Enterprise 26H2 evaluation, en-US; the hash is Microsoft's ("Verify the authenticity of
# a Windows 11 Enterprise Evaluation ISO file", https://aka.ms/VerifyWindowsISO).
WINDOWS_ISO_NAME = (
    "26300.9457.260913-1737.26h2_ge_release_svc_refresh_"
    "CLIENTENTERPRISEEVAL_OEMRET_x64FRE_en-us.iso"
)
WINDOWS_ISO_URL = f"https://software-static.download.prss.microsoft.com/dbazure/{WINDOWS_ISO_NAME}"
WINDOWS_ISO_SHA256 = "bc3f24086ebadc94489066b5ad78089e2cf5c3491e90e790bb81a2b199c10e38"

# Fedora's virtio-win release. Its CHECKSUM file gives the RPM's md5
# (66f65c16ab3e8dfe12c2855a0d1ac303); the sha256 is of that same file.
VIRTIO_VERSION = "0.1.302"
VIRTIO_RPM_NAME = f"virtio-win-{VIRTIO_VERSION}-1.noarch.rpm"
VIRTIO_RPM_URL = (
    "https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/archive-virtio/"
    f"virtio-win-{VIRTIO_VERSION}-1/{VIRTIO_RPM_NAME}"
)
VIRTIO_RPM_SHA256 = "2f58eea024e0221a873032761dc3ed3262409cd80860b7b27d3d5a0f206616c9"
# fedorapeople serves each connection at a few dozen kB/s.
VIRTIO_CONNECTIONS = 24

OVMF_DIR = Path("/usr/share/edk2/ovmf")
OVMF_CODE = OVMF_DIR / "OVMF_CODE_4M.secboot.qcow2"
# Microsoft's Secure Boot keys enrolled.
OVMF_VARS = OVMF_DIR / "OVMF_VARS_4M.secboot.qcow2"

TEST_USER = "tester"
# Throwaway credential of the disposable guest; it protects nothing.
TEST_PASSWORD = "opensu-test"
# Where the harness keeps its files in the guest.
GUEST_DIR = r"C:\opensu-vm"


@dataclass(frozen=True)
class WinConfig:
    root: Path
    shots: Path
    memory_mb: int = 6144
    cpus: int = 4
    disk_gb: int = 64
    width: int = 1280
    height: int = 800

    @property
    def downloads(self) -> Path:
        return self.root / "downloads"

    @property
    def base_image(self) -> Path:
        return self.root / "base.qcow2"

    @property
    def base_vars(self) -> Path:
        return self.root / "base-vars.qcow2"

    @property
    def base_tpm(self) -> Path:
        return self.root / "base-tpm"

    @property
    def run(self) -> Path:
        return self.root / "run"

    @property
    def overlay(self) -> Path:
        return self.run / "overlay.qcow2"

    @property
    def vars(self) -> Path:
        return self.run / "vars.qcow2"

    @property
    def tpm_state(self) -> Path:
        return self.run / "tpm"

    @property
    def tpm_socket(self) -> Path:
        return self.run / "tpm.sock"

    @property
    def tpm_pidfile(self) -> Path:
        return self.run / "swtpm.pid"

    @property
    def qmp_socket(self) -> Path:
        return self.run / "qmp.sock"

    @property
    def vnc_socket(self) -> Path:
        return self.run / "vnc.sock"

    @property
    def agent_socket(self) -> Path:
        return self.run / "qga.sock"

    @property
    def pidfile(self) -> Path:
        return self.run / "qemu.pid"

    @property
    def qemu_log(self) -> Path:
        return self.run / "qemu.log"

    @property
    def serial_log(self) -> Path:
        return self.run / "serial.log"


def load() -> WinConfig:
    root = Path(os.environ.get("OPENSU_WINDOWS_VM_DIR", "/mnt/Boy/vm/opensu-windows"))
    shots = Path(os.environ.get("OPENSU_VM_SHOTS", str(REPO / "scratch" / "vm" / "windows-shots")))
    return WinConfig(root=root, shots=shots)
