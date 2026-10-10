"""Build the current tree on the host, install it under the guest user's ~/.local, ship it."""

import os
import re
import shlex
import shutil
import subprocess
import tarfile
from pathlib import Path

from tools.vm import config as vmconfig
from tools.vm.config import REPO, TEST_USER, VmConfig
from tools.vm.guest import Guest

GUEST_PREFIX = f"/home/{TEST_USER}/.local"
FORK_BUILD = REPO / "build" / "clang"
SESSION_ENTRY = f"{GUEST_PREFIX}/share/wayland-sessions/opensu.desktop"
SESSION_ROOT_ENTRY = "/usr/local/share/wayland-sessions/opensu.desktop"
BUILD_DIR = REPO / "build" / "vm"
VM_TMP = vmconfig.load().root / "tmp"


def _cache_value(cache: Path, name: str) -> str:
    match = re.search(rf"^{name}:[A-Z]+=(.*)$", cache.read_text(), re.M)
    if match is None:
        raise RuntimeError(f"{name} is not in {cache}; configure {cache.parent} first")
    return match.group(1)


def build(fork_build: Path = FORK_BUILD) -> None:
    """Configure build/vm like the maintainer build (same raylib and Lucent) and build opensu."""
    cache = fork_build / "CMakeCache.txt"
    subprocess.run(
        ["cmake", "-S", str(REPO), "-B", str(BUILD_DIR), "-G", "Ninja",
         "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_C_COMPILER=clang",
         f"-DRAYLIB_ROOT={_cache_value(cache, 'RAYLIB_ROOT')}",
         f"-DLUCENT_ROOT={_cache_value(cache, 'LUCENT_ROOT')}",
         "-DOPENSU_BUILD_GAMESCOPE=OFF", f"-DCMAKE_INSTALL_PREFIX={GUEST_PREFIX}"],
        check=True,
        env=_environ(),
    )  # fmt: skip
    subprocess.run(
        ["cmake", "--build", str(BUILD_DIR), "--target", "opensu", "opensu_signin_xpi"],
        check=True,
        env=_environ(),
    )


# The VM has no scanout-capable GPU (see docs/vm-harness.md), so the DRM backend runs headless.
HEADLESS_SHIM = """#!/bin/sh
here=$(dirname "$0")
for arg in "$@"; do
  shift
  if [ "$arg" = drm ]; then arg=headless; fi
  set -- "$@" "$arg"
done
exec "$here/gamescope.real" "$@"
"""


def stage(cfg: VmConfig, fork: Path, headless_shim: bool = False) -> Path:
    """DESTDIR install of the build plus the pinned Gamescope fork, as one tarball."""
    if not fork.exists():
        raise RuntimeError(f"the Gamescope fork binary is missing at {fork}; build it first")
    root = cfg.run / "stage"
    shutil.rmtree(root, ignore_errors=True)
    env = {**_environ(), "DESTDIR": str(root)}
    subprocess.run(["cmake", "--install", str(BUILD_DIR)], check=True, env=env)
    prefix = root / GUEST_PREFIX.lstrip("/")
    libexec = prefix / "libexec" / "opensu"
    libexec.mkdir(parents=True, exist_ok=True)
    if headless_shim:
        shutil.copy2(fork, libexec / "gamescope.real")
        (libexec / "gamescope").write_text(HEADLESS_SHIM)
        (libexec / "gamescope").chmod(0o755)
    else:
        shutil.copy2(fork, libexec / "gamescope")
    tarball = cfg.run / "opensu-stage.tar"
    with tarfile.open(tarball, "w") as archive:
        archive.add(prefix, arcname=".")
    return tarball


def _environ() -> dict[str, str]:
    """Host builds keep ccache and compiler temporaries off the user's small /run/user tmpfs."""
    scratch = VM_TMP
    scratch.mkdir(parents=True, exist_ok=True)
    return {**os.environ, "CCACHE_TEMPDIR": str(scratch), "TMPDIR": str(scratch)}


def runtime_packages(binaries: list[Path]) -> list[str]:
    """Distribution packages owning the shared libraries the binaries load, from the host."""
    libs: set[str] = set()
    for binary in binaries:
        listing = subprocess.run(["ldd", str(binary)], capture_output=True, text=True, check=True)
        libs.update(re.findall(r"=> (/\S+)", listing.stdout))
    owners = subprocess.run(
        ["rpm", "-qf", "--qf", "%{NAME}\n", *sorted(libs)], capture_output=True, text=True,
        check=False,
    )  # fmt: skip
    names = {line for line in owners.stdout.splitlines() if line and " " not in line}
    return sorted(names)


def ship(cfg: VmConfig, guest: Guest, tarball: Path, packages: list[str]) -> None:
    guest.run(
        f"rpm -q {' '.join(map(shlex.quote, packages))} >/dev/null 2>&1 || "
        f"dnf -y install --setopt=install_weak_deps=False {' '.join(map(shlex.quote, packages))}",
        sudo=True, timeout=1800,
    )  # fmt: skip
    guest.run(f"mkdir -p {GUEST_PREFIX} && rm -rf {GUEST_PREFIX}/share/opensu")
    guest.put(tarball, "/tmp/opensu-stage.tar")
    guest.run(f"tar -C {GUEST_PREFIX} -xf /tmp/opensu-stage.tar && rm /tmp/opensu-stage.tar")


def install_session_entry(guest: Guest) -> str:
    """The README's `sudo install` line, run as the test user."""
    line = f"install -Dm644 {SESSION_ENTRY} {SESSION_ROOT_ENTRY}"
    return guest.run(line, sudo=True).out


def deploy(
    cfg: VmConfig, guest: Guest, fork: Path | None = None, headless_shim: bool = False
) -> None:
    fork = fork or FORK_BUILD / "libexec" / "opensu" / "gamescope"
    build()
    tarball = stage(cfg, fork, headless_shim)
    packages = runtime_packages([BUILD_DIR / "src" / "opensu", fork])
    ship(cfg, guest, tarball, [*packages, "tar"])
