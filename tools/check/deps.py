"""Fetching the pinned third-party sources."""

import subprocess
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from tools.check import process, windows
from tools.check.pins import GPU2D, LUCENT, GitPin


@dataclass(frozen=True)
class Deps:
    """Where the pinned sources live under one directory."""

    root: Path

    @property
    def lucent(self) -> Path:
        return self.root / "lucent"

    @property
    def gpu2d(self) -> Path:
        return self.root / "gpu2d"

    @property
    def vcpkg(self) -> Path:
        return self.root / "vcpkg"


def checkout(pin: GitPin, dest: Path, env: Mapping[str, str]) -> None:
    """`dest` holds exactly `pin.commit`; the object id is the checksum."""
    git = ["git", "-C", str(dest)]
    if not (dest / ".git").is_dir():
        dest.mkdir(parents=True, exist_ok=True)
        process.run([*git, "init", "--quiet"], env)
        process.run([*git, "remote", "add", "origin", pin.url], env)
    if _head(dest, env) == pin.commit:
        return
    process.run([*git, "remote", "set-url", "origin", pin.url], env)
    process.run([*git, "fetch", "--quiet", "--depth", "1", "origin", pin.commit], env)
    process.run([*git, "checkout", "--quiet", "--detach", pin.commit], env)
    head = _head(dest, env)
    if head != pin.commit:
        raise RuntimeError(f"{pin.name}: checked out {head}, pinned {pin.commit}")


def _head(dest: Path, env: Mapping[str, str]) -> str:
    """The checked-out commit, empty in a fresh clone."""
    result = subprocess.run(
        ["git", "-C", str(dest), "rev-parse", "--verify", "--quiet", "HEAD"],
        env=dict(env),
        capture_output=True,
        text=True,
        check=False,
    )
    return result.stdout.strip()


def fetch(deps: Deps, env: Mapping[str, str], platform: str = "linux") -> None:
    """Every pinned source the platform builds with in place."""
    checkout(LUCENT, deps.lucent, env)
    checkout(GPU2D, deps.gpu2d, env)
    if platform == "windows":
        checkout(windows.vcpkg_pin(), deps.vcpkg, env)
        windows.bootstrap(deps.vcpkg, env)
