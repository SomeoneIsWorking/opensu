"""The QEMU process: command line, launch, and stop by its recorded PID."""

import os
import shutil
import signal
import subprocess
import time
from pathlib import Path

from tools.vm.config import VmConfig
from tools.vm.qmp import Qmp

GPU_MODES = ("virtio", "venus")
RENDER_NODE = "/dev/dri/renderD128"


def gpu_args(mode: str, memory_mb: int) -> list[str]:
    """Display and GPU arguments. venus needs host virglrenderer+Vulkan and shared memory."""
    if mode == "virtio":
        return ["-display", "none", "-device", "virtio-gpu-pci"]
    if mode == "venus":
        return [
            "-object", f"memory-backend-memfd,id=mem1,size={memory_mb}M,share=on",
            "-machine", "q35,accel=kvm,memory-backend=mem1",
            "-display", f"egl-headless,rendernode={RENDER_NODE}",
            "-device", "virtio-gpu-gl-pci,blob=true,venus=true,hostmem=2G",
        ]  # fmt: skip
    raise ValueError(f"unknown gpu mode {mode}")


def vnc_args(cfg: VmConfig, gpu: str) -> list[str]:
    """egl-headless draws no console surface QMP could dump; a VNC socket carries the frames."""
    return ["-vnc", f"unix:{cfg.vnc_socket}"] if gpu == "venus" else []


def build_command(cfg: VmConfig, disk: Path, gpu: str, extra: list[str]) -> list[str]:
    machine = ["-machine", "q35,accel=kvm"] if gpu != "venus" else []
    return [
        "qemu-system-x86_64",
        "-name", "opensu-vm",
        "-cpu", "host", "-smp", str(cfg.cpus), "-m", str(cfg.memory_mb),
        *machine,
        "-drive", f"file={disk},if=virtio,format=qcow2,cache=writeback",
        "-nic", f"user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:{cfg.ssh_port}-:22",
        "-vga", "none", "-device", "qemu-xhci", "-device", "usb-kbd", "-device", "usb-tablet",
        *gpu_args(gpu, cfg.memory_mb),
        *vnc_args(cfg, gpu),
        "-qmp", f"unix:{cfg.qmp_socket},server=on,wait=off",
        "-serial", f"file:{cfg.serial_log}",
        "-pidfile", str(cfg.pidfile),
        *extra,
    ]  # fmt: skip


def running_pid(cfg: VmConfig) -> int | None:
    try:
        pid = int(cfg.pidfile.read_text())
    except (FileNotFoundError, ValueError):
        return None
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return None
    return pid


def launch(cfg: VmConfig, disk: Path, gpu: str, extra: list[str] | None = None) -> int:
    if running_pid(cfg) is not None:
        raise RuntimeError("the VM is already running; run `down` first")
    cfg.run.mkdir(parents=True, exist_ok=True)
    cfg.qmp_socket.unlink(missing_ok=True)
    cfg.vnc_socket.unlink(missing_ok=True)
    cfg.pidfile.unlink(missing_ok=True)
    command = build_command(cfg, disk, gpu, extra or [])
    with cfg.qemu_log.open("w") as log:
        process = subprocess.Popen(
            command, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
            start_new_session=True,
        )  # fmt: skip
    deadline = time.monotonic() + 15
    while not cfg.qmp_socket.exists():
        if process.poll() is not None:
            raise RuntimeError(f"qemu failed to start: {cfg.qemu_log.read_text()}")
        if time.monotonic() > deadline:
            raise RuntimeError("qemu started but QMP never appeared")
        time.sleep(0.2)
    pid = running_pid(cfg)
    assert pid is not None
    return pid


def stop(cfg: VmConfig, graceful_s: int = 60) -> None:
    """ACPI powerdown, then quit, then SIGKILL of the recorded PID."""
    pid = running_pid(cfg)
    if pid is None:
        return
    try:
        with Qmp(cfg.qmp_socket, cfg.width, cfg.height) as qmp:
            qmp.powerdown()
    except OSError:
        pass
    deadline = time.monotonic() + graceful_s
    while time.monotonic() < deadline and running_pid(cfg) is not None:
        time.sleep(1)
    if running_pid(cfg) is not None:
        try:
            with Qmp(cfg.qmp_socket, cfg.width, cfg.height) as qmp:
                qmp.quit()
        except OSError:
            os.kill(pid, signal.SIGKILL)
        time.sleep(1)
    cfg.pidfile.unlink(missing_ok=True)


def new_overlay(cfg: VmConfig, backing: Path) -> Path:
    cfg.run.mkdir(parents=True, exist_ok=True)
    cfg.overlay.unlink(missing_ok=True)
    subprocess.run(
        ["qemu-img", "create", "-q", "-f", "qcow2", "-b", str(backing), "-F", "qcow2",
         str(cfg.overlay)],
        check=True,
    )  # fmt: skip
    return cfg.overlay


def flatten(source: Path, target: Path) -> None:
    subprocess.run(["qemu-img", "convert", "-O", "qcow2", str(source), str(target)], check=True)


def require_tools() -> None:
    for tool in ("qemu-system-x86_64", "qemu-img", "xorriso", "ssh", "scp"):
        if shutil.which(tool) is None:
            raise RuntimeError(
                f"{tool} is missing; install it (sudo dnf install qemu-kvm xorriso openssh-clients)"
            )


def wait_exit(cfg: VmConfig, timeout_s: int) -> None:
    deadline = time.monotonic() + timeout_s
    while running_pid(cfg) is not None:
        if time.monotonic() > deadline:
            raise RuntimeError(f"the VM was still running after {timeout_s}s")
        time.sleep(10)
