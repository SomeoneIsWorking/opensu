"""Checksum-verified downloads shared by the maintainer tools."""

import hashlib
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

USER_AGENT = "opensu-tools/1 (https://github.com/SomeoneIsWorking/opensu)"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _request(url: str, headers: dict[str, str] | None = None) -> urllib.request.Request:
    return urllib.request.Request(url, headers={"User-Agent": USER_AGENT, **(headers or {})})


def _content_length(url: str) -> int:
    with urllib.request.urlopen(_request(url), timeout=120) as response:
        return int(response.headers["Content-Length"])


def _fetch_range(url: str, part: Path, start: int, end: int) -> None:
    """Bytes start..end (inclusive) of url into their place in part."""
    request = _request(url, {"Range": f"bytes={start}-{end}"})
    with urllib.request.urlopen(request, timeout=120) as response, part.open("r+b") as out:
        if response.status != 206:
            raise RuntimeError(f"{url}: no range support (HTTP {response.status})")
        out.seek(start)
        for block in iter(lambda: response.read(1 << 20), b""):
            out.write(block)


def _fetch_whole(url: str, part: Path) -> None:
    with urllib.request.urlopen(_request(url), timeout=120) as response, part.open("wb") as out:
        for block in iter(lambda: response.read(1 << 20), b""):
            out.write(block)


def fetch_verified(url: str, sha256: str, target: Path, connections: int = 1) -> Path:
    """The file at target, downloaded from url when missing or different; refuses a mismatch.
    `connections` above one fetches that many byte ranges at once, for a server that throttles
    each connection."""
    if target.is_file() and sha256_file(target) == sha256:
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    part = target.with_name(target.name + ".part")
    if connections > 1:
        size = _content_length(url)
        with part.open("wb") as out:
            out.truncate(size)
        step = -(-size // connections)
        ranges = [(start, min(start + step, size) - 1) for start in range(0, size, step)]
        with ThreadPoolExecutor(max_workers=connections) as pool:
            for future in [pool.submit(_fetch_range, url, part, a, b) for a, b in ranges]:
                future.result()
    else:
        _fetch_whole(url, part)
    actual = sha256_file(part)
    if actual != sha256:
        part.unlink()
        raise RuntimeError(f"{url}: sha256 {actual}, expected {sha256}")
    part.replace(target)
    return target
