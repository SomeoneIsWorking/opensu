"""The install media: Microsoft's ISO and the virtio-win ISO from Fedora's RPM, both checksummed."""

import subprocess
from pathlib import Path

from tools.download import fetch_verified
from tools.vm.windows import config


def windows_iso(downloads: Path) -> Path:
    return fetch_verified(
        config.WINDOWS_ISO_URL, config.WINDOWS_ISO_SHA256, downloads / config.WINDOWS_ISO_NAME
    )


def virtio_iso(downloads: Path) -> Path:
    """The ISO inside the verified virtio-win RPM, unpacked beside it."""
    iso = downloads / f"virtio-win-{config.VIRTIO_VERSION}.iso"
    rpm = fetch_verified(
        config.VIRTIO_RPM_URL,
        config.VIRTIO_RPM_SHA256,
        downloads / config.VIRTIO_RPM_NAME,
        connections=config.VIRTIO_CONNECTIONS,
    )
    if iso.is_file():
        return iso
    member = f"./usr/share/virtio-win/{iso.name}"
    part = iso.with_name(iso.name + ".part")
    with (
        subprocess.Popen(["rpm2cpio", str(rpm)], stdout=subprocess.PIPE) as unpack,
        part.open("wb") as out,
    ):
        subprocess.run(
            ["cpio", "-i", "--quiet", "--to-stdout", member],
            stdin=unpack.stdout,
            stdout=out,
            check=True,
        )
    if part.stat().st_size == 0:
        part.unlink()
        raise RuntimeError(f"{rpm} holds no {member}")
    part.replace(iso)
    return iso
