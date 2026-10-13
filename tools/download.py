"""Checksum-verified downloads shared by the maintainer tools."""

import hashlib
import urllib.request
from pathlib import Path

USER_AGENT = "opensu-tools/1 (https://github.com/SomeoneIsWorking/opensu)"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def fetch_verified(url: str, sha256: str, target: Path) -> Path:
    """The file at target, downloaded from url when missing or different; refuses a mismatch."""
    if target.is_file() and sha256_file(target) == sha256:
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    part = target.with_name(target.name + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=120) as response, part.open("wb") as out:
        for block in iter(lambda: response.read(1 << 20), b""):
            out.write(block)
    actual = sha256_file(part)
    if actual != sha256:
        part.unlink()
        raise RuntimeError(f"{url}: sha256 {actual}, expected {sha256}")
    part.replace(target)
    return target
