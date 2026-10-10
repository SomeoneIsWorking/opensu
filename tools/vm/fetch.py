"""Checksum-verified downloads of the installer kernel and initrd."""

import hashlib
import urllib.request
from pathlib import Path

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


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _get(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def fetch_installer(downloads: Path) -> tuple[Path, Path]:
    """Download kernel and initrd into downloads/, verified against the tree's .treeinfo."""
    downloads.mkdir(parents=True, exist_ok=True)
    sums = parse_treeinfo_checksums(_get(f"{TREE_URL}/.treeinfo").decode())
    out = []
    for rel in PXE_FILES:
        expected = sums[rel]
        target = downloads / Path(rel).name
        if not target.exists() or sha256_file(target) != expected:
            part = target.with_suffix(target.suffix + ".part")
            part.write_bytes(_get(f"{TREE_URL}/{rel}"))
            if sha256_file(part) != expected:
                part.unlink()
                raise RuntimeError(f"checksum mismatch for {rel}")
            part.replace(target)
        out.append(target)
    return out[0], out[1]
