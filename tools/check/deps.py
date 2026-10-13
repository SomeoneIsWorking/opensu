"""Fetching the pinned third-party sources and building raylib into a prefix of its own."""

import subprocess
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from tools.check import process
from tools.check.pins import LUCENT, RAYLIB, RAYLIB_CMAKE_OPTIONS, GitPin


@dataclass(frozen=True)
class Deps:
    """Where the pinned sources live under one directory."""

    root: Path

    @property
    def raylib_source(self) -> Path:
        return self.root / "raylib"

    @property
    def raylib_build(self) -> Path:
        return self.root / "raylib-build"

    @property
    def raylib_prefix(self) -> Path:
        return self.root / "raylib-prefix"

    @property
    def lucent(self) -> Path:
        return self.root / "lucent"


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


def build_raylib(deps: Deps, env: Mapping[str, str]) -> None:
    """Build and install raylib into `deps.raylib_prefix`; an unchanged rerun does nothing."""
    if not (deps.raylib_build / "build.ninja").exists():
        process.run(
            [
                "cmake",
                "-S",
                deps.raylib_source,
                "-B",
                deps.raylib_build,
                "-G",
                "Ninja",
                "-DCMAKE_C_COMPILER=clang",
                *RAYLIB_CMAKE_OPTIONS,
            ],
            env,
        )
    process.run(["cmake", "--build", deps.raylib_build], env)
    process.run(["cmake", "--install", deps.raylib_build, "--prefix", deps.raylib_prefix], env)


def fetch(deps: Deps, env: Mapping[str, str]) -> None:
    """Every pinned source in place and raylib installed."""
    checkout(RAYLIB, deps.raylib_source, env)
    checkout(LUCENT, deps.lucent, env)
    build_raylib(deps, env)
