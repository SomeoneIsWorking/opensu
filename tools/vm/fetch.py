"""Checksum-verified downloads of the installer kernel and initrd."""

import urllib.request
from pathlib import Path

from tools.download import fetch_verified
from tools.vm.config import TREE_URL

PXE_FILES = ("images/pxeboot/vmlinuz", "images/pxeboot/initrd.img")


def parse_treeinfo_checksums(text: str) -> dict[str, str]:
    """The [checksums] section of a Fedora .treeinfo as path -> sha256 hex."""
    sums: dict[str, str] = {}
    in_section = False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("["):
            in_section = line == "[checksums]"
        elif in_section and "=" in line:
            path, _, value = line.partition("=")
            algo, _, digest = value.strip().partition(":")
            if algo == "sha256":
                sums[path.strip()] = digest
    return sums


def _get(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def fetch_installer(downloads: Path) -> tuple[Path, Path]:
    """Download kernel and initrd into downloads/, verified against the tree's .treeinfo."""
    downloads.mkdir(parents=True, exist_ok=True)
    sums = parse_treeinfo_checksums(_get(f"{TREE_URL}/.treeinfo").decode())
    kernel, initrd = (
        fetch_verified(f"{TREE_URL}/{rel}", sums[rel], downloads / Path(rel).name)
        for rel in PXE_FILES
    )
    return kernel, initrd
