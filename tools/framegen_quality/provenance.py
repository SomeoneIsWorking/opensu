"""Which build made a run's frames: the generator binary's hash and the commit and uncommitted
diff of the checkout it was built from (the build tree's CMake source directory), written beside
the run. A binary older than its own generator sources is a build that did not finish, and the
run is refused rather than scored with a stale generator."""

import hashlib
import json
import subprocess
from pathlib import Path

_SOURCES = ("src/frame_gen",)


def _source_dir(build: Path) -> Path:
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line.startswith("CMAKE_HOME_DIRECTORY:"):
            return Path(line.split("=", 1)[1])
    raise SystemExit(f"{build}/CMakeCache.txt names no source directory")


def _git(source: Path, *args: str) -> bytes:
    return subprocess.run(["git", "-C", str(source), *args], check=True, capture_output=True).stdout


def check(build: Path, binary: Path) -> dict[str, str]:
    """The build's identity; exits when `binary` is older than a generator source."""
    source = _source_dir(build)
    built = binary.stat().st_mtime
    for folder in _SOURCES:
        for path in (source / folder).rglob("*"):
            if path.is_file() and path.stat().st_mtime > built:
                raise SystemExit(
                    f"{binary} is older than {path}: the build did not finish, build it again"
                )
    diff = _git(source, "diff", "HEAD", "--", *_SOURCES, "tools")
    return {
        "binary": str(binary),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "source": str(source),
        "commit": _git(source, "rev-parse", "HEAD").decode().strip(),
        "diff_sha256": hashlib.sha256(diff).hexdigest(),
    }


def save(identity: dict[str, str], run: Path) -> None:
    run.mkdir(parents=True, exist_ok=True)
    (run / "build.json").write_text(json.dumps(identity, indent=1) + "\n")
