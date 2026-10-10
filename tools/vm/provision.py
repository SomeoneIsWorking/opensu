"""Unattended install of the Fedora KDE guest, then the configuration that mirrors the host."""

import subprocess

from tools.vm import configure, fetch, kickstart, machine
from tools.vm.config import FEDORA_RELEASE, TREE_URL, VmConfig
from tools.vm.guest import Guest


def ensure_ssh_key(cfg: VmConfig) -> str:
    cfg.root.mkdir(parents=True, exist_ok=True)
    if not cfg.ssh_key.exists():
        subprocess.run(
            [
                "ssh-keygen",
                "-q",
                "-t",
                "ed25519",
                "-N",
                "",
                "-C",
                "opensu-vm",
                "-f",
                str(cfg.ssh_key),
            ],
            check=True,
        )
    return cfg.ssh_key.with_suffix(".pub").read_text().strip()


def install(cfg: VmConfig) -> None:
    """Boot the installer with the kickstart; qemu exits when the guest powers off."""
    machine.require_tools()
    if cfg.base_image.exists():
        raise RuntimeError(f"{cfg.base_image} exists; delete it to provision again")
    kernel, initrd = fetch.fetch_installer(cfg.downloads)
    text = kickstart.render(ensure_ssh_key(cfg), FEDORA_RELEASE, cfg.disk_gb)
    oemdrv = kickstart.build_oemdrv(cfg.run, text)
    disk = cfg.root / "install.qcow2"
    disk.unlink(missing_ok=True)
    subprocess.run(
        ["qemu-img", "create", "-q", "-f", "qcow2", str(disk), f"{cfg.disk_gb}G"], check=True
    )
    append = (
        f"inst.repo={TREE_URL} inst.ks=hd:LABEL=OEMDRV:/ks.cfg inst.text "
        "inst.notmux console=ttyS0 inst.nosave=all"
    )
    extra = [
        "-kernel", str(kernel), "-initrd", str(initrd), "-append", append,
        "-drive", f"file={oemdrv},if=none,id=oem,format=raw,readonly=on,media=cdrom",
        "-device", "virtio-scsi-pci", "-device", "scsi-cd,drive=oem",
        "-no-reboot",
    ]  # fmt: skip
    machine.launch(cfg, disk, "virtio", extra)
    cfg.run.joinpath("install.disk").write_text(str(disk))


def finish(cfg: VmConfig) -> None:
    """Boot the installed disk, apply the host-like session setup, save it as base.qcow2."""
    installed = cfg.root / "install.qcow2"
    machine.launch(cfg, installed, "virtio")
    guest = Guest(cfg)
    guest.wait_ready()
    configure.apply_host_session_setup(guest)
    configure.upgrade(guest)
    guest.run("systemctl --no-block poweroff", sudo=True)
    machine.wait_exit(cfg, 120)
    installed.rename(cfg.base_image)
