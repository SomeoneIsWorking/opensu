"""The unattended install: kickstart text and the OEMDRV disk that carries it."""

import subprocess
from pathlib import Path

from tools.vm.config import MIRRORLIST, TEST_PASSWORD, TEST_USER

PACKAGES = (
    "@core",
    "plasma-desktop",
    "plasma-workspace",
    "plasma-workspace-wallpapers",
    "sddm",
    "sddm-breeze",
    "sddm-kcm",
    "sddm-wayland-plasma",
    "konsole",
    "NetworkManager",
    "openssh-server",
    "sudo",
    "gamescope",
    "mesa-vulkan-drivers",
    "vulkan-tools",
    "xdg-utils",
    "tar",
    "psmisc",
    "procps-ng",
    "qemu-guest-agent",
)


def render(public_key: str, release: str, disk_gb: int) -> str:
    packages = "\n".join(PACKAGES)
    repo_args = f"?repo=fedora-{release}&arch=x86_64"
    updates_args = f"?repo=updates-released-f{release}&arch=x86_64"
    return f"""text
poweroff
lang en_US.UTF-8
keyboard us
timezone UTC
rootpw --lock
user --name={TEST_USER} --groups=wheel --password={TEST_PASSWORD} --plaintext
url --mirrorlist="{MIRRORLIST}{repo_args}"
repo --name=updates --mirrorlist="{MIRRORLIST}{updates_args}"
network --bootproto=dhcp --device=link --activate --hostname=opensu-vm
firstboot --disable
selinux --enforcing
zerombr
clearpart --all --initlabel --disklabel=msdos
bootloader --location=mbr --append="console=ttyS0 quiet"
part / --fstype=ext4 --size=1024 --grow
services --enabled=sshd,sddm,NetworkManager,qemu-guest-agent
skipx

%packages
{packages}
%end

%post --erroronfail
install -d -m 700 -o {TEST_USER} -g {TEST_USER} /home/{TEST_USER}/.ssh
echo '{public_key}' > /home/{TEST_USER}/.ssh/authorized_keys
chown {TEST_USER}:{TEST_USER} /home/{TEST_USER}/.ssh/authorized_keys
chmod 600 /home/{TEST_USER}/.ssh/authorized_keys
restorecon -R /home/{TEST_USER}/.ssh
systemctl set-default graphical.target
%end
"""


def build_oemdrv(workdir: Path, text: str) -> Path:
    """An ISO labelled OEMDRV holding ks.cfg, which Anaconda picks up on its own."""
    workdir.mkdir(parents=True, exist_ok=True)
    tree = workdir / "oemdrv"
    tree.mkdir(exist_ok=True)
    (tree / "ks.cfg").write_text(text)
    iso = workdir / "oemdrv.iso"
    subprocess.run(
        ["xorriso", "-as", "mkisofs", "-quiet", "-V", "OEMDRV", "-o", str(iso), str(tree)],
        check=True,
    )
    return iso
