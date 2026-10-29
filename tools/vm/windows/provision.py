"""The unattended Windows install, saved as the base image every run starts from."""

import shutil
import subprocess
import time

from tools.vm import machine as vm_machine
from tools.vm.qmp import Qmp
from tools.vm.windows import machine, media, unattend
from tools.vm.windows.config import OVMF_VARS, WinConfig

# The ISO's boot loader waits a few seconds for a key before it boots from the CD.
BOOT_KEY_WINDOW_S = 60


def _install_disk(cfg: WinConfig):
    return cfg.root / "install.qcow2"


def install(cfg: WinConfig) -> None:
    """Boots Setup with the answer file; qemu exits when the first logon powers the guest off."""
    machine.require_tools()
    if cfg.base_image.exists():
        raise RuntimeError(f"{cfg.base_image} exists; delete it to provision again")
    windows = media.windows_iso(cfg.downloads)
    virtio = media.virtio_iso(cfg.downloads)
    answers = unattend.build_iso(cfg.run)
    disk = _install_disk(cfg)
    disk.unlink(missing_ok=True)
    subprocess.run(
        ["qemu-img", "create", "-q", "-f", "qcow2", str(disk), f"{cfg.disk_gb}G"], check=True
    )
    machine.fresh_run_state(cfg, OVMF_VARS, None)
    machine.launch(cfg, disk, [windows, virtio, answers])
    deadline = time.monotonic() + BOOT_KEY_WINDOW_S
    with Qmp(cfg.qmp_socket, cfg.width, cfg.height) as qmp:
        while time.monotonic() < deadline:
            qmp.send_key(["spc"])
            time.sleep(1)


def wait(cfg: WinConfig, timeout_s: int) -> None:
    """Waits for the install to power off, keeping a screenshot of its progress in the run dir."""
    deadline = time.monotonic() + timeout_s
    while vm_machine.running_pid(cfg) is not None:
        if time.monotonic() > deadline:
            raise RuntimeError(f"the install still ran after {timeout_s}s")
        try:
            with Qmp(cfg.qmp_socket, cfg.width, cfg.height) as qmp:
                qmp.screendump(cfg.run / "install-progress.png")
        except OSError:
            pass
        time.sleep(30)
    machine.stop_tpm(cfg)


def finish(cfg: WinConfig) -> None:
    """The installed disk, its firmware variables and its TPM become the base."""
    _install_disk(cfg).rename(cfg.base_image)
    shutil.copyfile(cfg.vars, cfg.base_vars)
    if cfg.base_tpm.exists():
        shutil.rmtree(cfg.base_tpm)
    shutil.copytree(cfg.tpm_state, cfg.base_tpm)
