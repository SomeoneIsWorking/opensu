"""Which libraries the AppImage carries, copying them in and pointing every ELF at them.

Runs inside the build container, where ldd resolves what the build linked against. The host
provides the C and C++ runtimes, the GPU stack (GL, EGL, GBM, DRM, Vulkan), core X11 and the
system libraries every desktop has; everything else the binaries need is bundled.
"""

import re
import shutil
from collections.abc import Mapping, Sequence
from pathlib import Path

from tools.check import process

HOST_PROVIDED = frozenset(
    {
        # glibc
        "ld-linux-x86-64.so.2", "libc.so.6", "libm.so.6", "libdl.so.2", "libpthread.so.0",
        "librt.so.1", "libresolv.so.2", "libutil.so.1", "libmvec.so.1", "libanl.so.1",
        # compiler runtime, which the host's GPU drivers also load
        "libstdc++.so.6", "libgcc_s.so.1",
        # GPU stack: must match the host's kernel driver and Mesa or vendor driver
        "libGL.so.1", "libGLX.so.0", "libEGL.so.1", "libGLdispatch.so.0", "libGLESv2.so.2",
        "libOpenGL.so.0", "libgbm.so.1", "libdrm.so.2", "libvulkan.so.1",
        # core X11, shared with the host's GL driver
        "libX11.so.6", "libX11-xcb.so.1", "libxcb.so.1", "libXau.so.6", "libXdmcp.so.6",
        # system libraries tied to the host's daemons, plugins or trust store
        "libudev.so.1", "libsystemd.so.0", "libz.so.1", "libcurl.so.4", "libcrypto.so.3",
        "libpipewire-0.3.so.0", "libglib-2.0.so.0", "libgobject-2.0.so.0", "libgio-2.0.so.0",
        "libgmodule-2.0.so.0",
        "libffi.so.8", "libexpat.so.1", "libcap.so.2", "libdbus-1.so.3", "libasound.so.2",
        "libpulse.so.0",
    }
)  # fmt: skip

LDD_LINE = re.compile(r"^\s*(\S+)\s+=>\s+(\S+)")
GLIBC_VERSION = re.compile(r"GLIBC_(\d+)\.(\d+)")


def is_elf(path: Path) -> bool:
    if not path.is_file() or path.is_symlink():
        return False
    with path.open("rb") as handle:
        return handle.read(4) == b"\x7fELF"


def elf_files(root: Path) -> list[Path]:
    return sorted(path for path in root.rglob("*") if is_elf(path))


def needed(path: Path, env: Mapping[str, str]) -> list[str]:
    return process.output(["patchelf", "--print-needed", path], env).split()


def resolved(path: Path, env: Mapping[str, str]) -> dict[str, str]:
    """soname -> path as the dynamic loader finds it for `path`; missing ones map to ''."""
    found: dict[str, str] = {}
    for line in process.output(["ldd", path], env).splitlines():
        match = LDD_LINE.match(line)
        if match:
            found[match.group(1)] = "" if match.group(2) == "not" else match.group(2)
    return found


def closure(roots: Sequence[Path], env: Mapping[str, str]) -> dict[str, Path]:
    """Every library the roots reach that the host does not provide, by soname."""
    bundled: dict[str, Path] = {}
    pending = list(roots)
    while pending:
        elf = pending.pop()
        where = resolved(elf, env)
        for soname in needed(elf, env):
            if soname in HOST_PROVIDED or soname in bundled:
                continue
            path = where.get(soname, "")
            if not path:
                raise RuntimeError(f"{elf} needs {soname}, which the build environment lacks")
            bundled[soname] = Path(path)
            pending.append(Path(path))
    return bundled


def runpath_for(elf: Path, usr: Path) -> str:
    """$ORIGIN-relative path from `elf`'s directory to usr/lib."""
    depth = len(elf.parent.relative_to(usr).parts)
    return "$ORIGIN/" + "/".join([".."] * depth + ["lib"]) if depth else "$ORIGIN/lib"


def max_glibc(elf: Path, env: Mapping[str, str]) -> tuple[int, int]:
    text = process.output(["readelf", "--version-info", "--wide", elf], env)
    versions = [(int(a), int(b)) for a, b in GLIBC_VERSION.findall(text)]
    return max(versions, default=(0, 0))


def bundle(usr: Path, glibc_floor: tuple[int, int], env: Mapping[str, str]) -> list[str]:
    """Copy the closure of usr/'s executables into usr/lib and set every ELF's runpath.

    Returns the bundled sonames. Fails when an ELF needs a glibc newer than the floor.
    """
    lib = usr / "lib"
    lib.mkdir(parents=True, exist_ok=True)
    programs = elf_files(usr)
    libraries = closure(programs, env)
    for soname, source in sorted(libraries.items()):
        target = lib / soname
        shutil.copy2(source.resolve(), target)
        target.chmod(0o755)
    for elf in elf_files(usr):
        if elf.parent == lib:
            process.run(["patchelf", "--set-rpath", "$ORIGIN", elf], env)
        else:
            process.run(["patchelf", "--set-rpath", runpath_for(elf, usr), elf], env)
        newest = max_glibc(elf, env)
        if newest > glibc_floor:
            raise RuntimeError(
                f"{elf.relative_to(usr)} needs GLIBC_{newest[0]}.{newest[1]}, above the floor "
                f"{glibc_floor[0]}.{glibc_floor[1]}"
            )
    return sorted(libraries)
