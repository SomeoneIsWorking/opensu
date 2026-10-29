"""The Windows guest's qemu: Secure Boot firmware, a software TPM 2.0, the guest agent's port."""

import os
import shutil
import signal
import subprocess
import time
from pathlib import Path

from tools.vm import machine
from tools.vm.windows.config import OVMF_CODE, WinConfig


def require_tools() -> None:
    for tool in ("qemu-system-x86_64", "qemu-img", "swtpm", "xorriso", "rpm2cpio", "cpio"):
        if shutil.which(tool) is None:
            raise RuntimeError(
                f"{tool} is missing; install it "
                "(sudo dnf install qemu-kvm swtpm swtpm-tools edk2-ovmf xorriso rpm cpio)"
            )
    if not OVMF_CODE.is_file():
        raise RuntimeError(f"{OVMF_CODE} is missing; install it (sudo dnf install edk2-ovmf)")


def start_tpm(cfg: WinConfig) -> None:
    """swtpm on the run's state; it ends itself when qemu lets go of the socket."""
    cfg.tpm_socket.unlink(missing_ok=True)
    cfg.tpm_pidfile.unlink(missing_ok=True)
    cfg.tpm_state.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["swtpm", "socket", "--tpm2", "--tpmstate", f"dir={cfg.tpm_state}",
         "--ctrl", f"type=unixio,path={cfg.tpm_socket}", "--terminate", "--daemon",
         "--pid", f"file={cfg.tpm_pidfile}", "--log", f"file={cfg.run / 'swtpm.log'}"],
        check=True,
    )  # fmt: skip
    deadline = time.monotonic() + 10
    while not cfg.tpm_socket.exists():
        if time.monotonic() > deadline:
            raise RuntimeError("swtpm never opened its socket")
        time.sleep(0.1)


def stop_tpm(cfg: WinConfig) -> None:
    try:
        pid = int(cfg.tpm_pidfile.read_text())
        os.kill(pid, signal.SIGTERM)
    except (FileNotFoundError, ValueError, ProcessLookupError):
        pass
    cfg.tpm_pidfile.unlink(missing_ok=True)


def build_command(cfg: WinConfig, disk: Path, cdroms: list[Path]) -> list[str]:
    drives: list[str] = []
    for index, iso in enumerate(cdroms):
        drives += [
            "-drive", f"file={iso},if=none,id=cd{index},format=raw,readonly=on,media=cdrom",
            "-device", f"ide-cd,drive=cd{index},bus=ide.{index},bootindex={index + 1}",
        ]  # fmt: skip
    return [
        "qemu-system-x86_64",
        "-name", "opensu-windows",
        "-machine", "q35,smm=on,accel=kvm",
        "-global", "driver=cfi.pflash01,property=secure,value=on",
        "-drive", f"if=pflash,format=qcow2,unit=0,readonly=on,file={OVMF_CODE}",
        "-drive", f"if=pflash,format=qcow2,unit=1,file={cfg.vars}",
        "-cpu", "host,hv_relaxed,hv_spinlocks=0x1fff,hv_vapic,hv_time",
        "-smp", str(cfg.cpus), "-m", str(cfg.memory_mb),
        "-rtc", "base=utc,clock=host",
        "-chardev", f"socket,id=chrtpm,path={cfg.tpm_socket}",
        "-tpmdev", "emulator,id=tpm0,chardev=chrtpm", "-device", "tpm-tis,tpmdev=tpm0",
        "-drive", f"file={disk},if=none,id=disk0,format=qcow2,cache=writeback,discard=unmap",
        "-device", f"virtio-blk-pci,drive=disk0,bootindex={len(cdroms) + 1}",
        *drives,
        "-nic", "user,model=virtio-net-pci",
        "-device", f"VGA,edid=on,xres={cfg.width},yres={cfg.height}",
        "-display", "none",
        "-device", "qemu-xhci", "-device", "usb-kbd", "-device", "usb-tablet",
        "-chardev", f"socket,id=qga0,path={cfg.agent_socket},server=on,wait=off",
        "-device", "virtio-serial-pci",
        "-device", "virtserialport,chardev=qga0,name=org.qemu.guest_agent.0",
        "-qmp", f"unix:{cfg.qmp_socket},server=on,wait=off",
        "-serial", f"file:{cfg.serial_log}",
        "-pidfile", str(cfg.pidfile),
    ]  # fmt: skip


def launch(cfg: WinConfig, disk: Path, cdroms: list[Path] | None = None) -> int:
    cfg.agent_socket.unlink(missing_ok=True)
    start_tpm(cfg)
    try:
        return machine.spawn(cfg, build_command(cfg, disk, cdroms or []))
    except RuntimeError:
        stop_tpm(cfg)
        raise


def fresh_run_state(cfg: WinConfig, vars_from: Path, tpm_from: Path | None) -> None:
    """The run's own firmware variables and TPM, copied so a run never changes the base's."""
    cfg.run.mkdir(parents=True, exist_ok=True)
    cfg.vars.unlink(missing_ok=True)
    shutil.copyfile(vars_from, cfg.vars)
    if cfg.tpm_state.exists():
        shutil.rmtree(cfg.tpm_state)
    if tpm_from is not None:
        shutil.copytree(tpm_from, cfg.tpm_state)


def stop(cfg: WinConfig) -> None:
    machine.stop(cfg)
    stop_tpm(cfg)
