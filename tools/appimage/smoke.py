"""The built AppImage on clean distributions: each a container with only the documented host
requirements installed, no network, the user's X server for the hidden render window.

Checks, per distribution: the runtime is the static type 2 one and libfuse2 is absent; the
AppImage mounts through FUSE and runs; every bundled ELF resolves every library; the Gamescope fork
binds every symbol at load; `opensu --render` draws a frame; and the run writes only under HOME
and the XDG directories.
"""

import struct
import subprocess
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

from tools import podman
from tools.appimage import bundle, config
from tools.check import process

ELF_INTERP = 3
RENDER_SIZE = (1280, 800)


@dataclass(frozen=True)
class Distro:
    image: str
    install: str
    packages: tuple[str, ...]


# The host requirements the README lists, per distribution, and nothing more.
DISTROS = {
    "fedora-44": Distro(
        "registry.fedoraproject.org/fedora:44"
        "@sha256:959833699a2fb5da1bc70b4db659ab229252c2ba1ed8e2fa29d4b30a704c0f63",
        "dnf -y install --setopt=install_weak_deps=False {packages} && dnf clean all",
        (
            "mesa-dri-drivers", "mesa-vulkan-drivers", "mesa-libgbm", "vulkan-loader",
            "libglvnd-glx", "libglvnd-egl", "libdrm", "libX11", "libX11-xcb", "libxcb",
            "libcurl", "glib2", "pipewire-libs", "systemd-libs", "libcap", "expat", "libffi",
            "zlib-ng-compat", "libstdc++", "dbus-libs", "alsa-lib", "pulseaudio-libs", "fuse3",
            "xkeyboard-config",
            "ca-certificates", "findutils",
        ),
    ),
    "ubuntu-24.04": Distro(
        "docker.io/library/ubuntu:24.04"
        "@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55",
        "apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y "
        "--no-install-recommends {packages} && rm -rf /var/lib/apt/lists/*",
        (
            "libgl1", "libegl1", "libgl1-mesa-dri", "mesa-vulkan-drivers", "libgbm1",
            "libvulkan1", "libdrm2", "libx11-6", "libx11-xcb1", "libxcb1", "libcurl4t64",
            "libglib2.0-0t64", "libpipewire-0.3-0t64", "libudev1", "libsystemd0", "libcap2",
            "libexpat1", "libffi8", "zlib1g", "libstdc++6", "libdbus-1-3", "libasound2t64",
            "libpulse0", "fuse3", "xkb-data",
            "ca-certificates",
        ),
    ),
}  # fmt: skip


def check_runtime(appimage: Path) -> None:
    """Type 2 magic at offset 8, and a runtime ELF without an interpreter (static)."""
    with appimage.open("rb") as handle:
        header = handle.read(64)
        if header[:4] != b"\x7fELF" or header[8:11] != b"AI\x02":
            raise RuntimeError(f"{appimage.name} is not a type 2 AppImage")
        (phoff,) = struct.unpack_from("<Q", header, 0x20)
        phentsize, phnum = struct.unpack_from("<HH", header, 0x36)
        handle.seek(phoff)
        table = handle.read(phentsize * phnum)
    for index in range(phnum):
        (kind,) = struct.unpack_from("<I", table, index * phentsize)
        if kind == ELF_INTERP:
            raise RuntimeError(f"{appimage.name}'s runtime is dynamically linked")


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"{path} is not a PNG")
    return struct.unpack(">II", data[16:24])


def smoke_image(name: str, distro: Distro, env: Mapping[str, str]) -> str:
    packages = " ".join(distro.packages)
    recipe = f"FROM {distro.image}\nRUN {distro.install.format(packages=packages)}\n"
    tag = podman.content_tag(f"localhost/opensu-appimage-smoke-{name}", recipe)
    return podman.ensure_image(tag, env, recipe=recipe)


class Container:
    """One running test container, removed by its own id."""

    def __init__(self, tag: str, appimage: Path, env: Mapping[str, str]) -> None:
        self.env = env
        words = [
            "podman", "run", "-d", "--rm", "--network", "none", "--device", "/dev/fuse",
            "--cap-add", "SYS_ADMIN", "--security-opt", "label=disable",
            "-v", f"{appimage.parent}:/appimage:ro", "-v", "/tmp/.X11-unix:/tmp/.X11-unix:ro",
        ]  # fmt: skip
        if Path("/dev/dri").exists():
            words += ["--device", "/dev/dri"]
        # The gamepad watcher needs /dev/input, which every desktop host has.
        words += ["-v", "/dev/input:/dev/input:ro"]
        display = env.get("DISPLAY", "")
        if not display:
            raise RuntimeError("the render check needs DISPLAY (CI wraps the job in xvfb-run)")
        words += ["-e", f"DISPLAY={display}"]
        xauthority = env.get("XAUTHORITY", "")
        if xauthority:
            words += ["-v", f"{xauthority}:/xauthority:ro", "-e", "XAUTHORITY=/xauthority"]
        self.id = process.output([*words, tag, "sleep", "infinity"], env).strip()

    def run(self, command: str, check: bool = True) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(
            ["podman", "exec", self.id, "sh", "-c", command],
            env=dict(self.env), capture_output=True, text=True, check=False,
        )  # fmt: skip
        if check and result.returncode != 0:
            raise RuntimeError(
                f"`{command}` failed ({result.returncode}):\n{result.stdout}{result.stderr}"
            )
        return result

    def copy_out(self, path: str, target: Path) -> None:
        target.parent.mkdir(parents=True, exist_ok=True)
        process.run(["podman", "cp", f"{self.id}:{path}", target], self.env)

    def remove(self) -> None:
        subprocess.run(["podman", "rm", "-f", "-t", "0", self.id], env=dict(self.env),
                       capture_output=True, check=False)  # fmt: skip


def bundled_elves(appdir: Path) -> list[str]:
    return [str(path.relative_to(appdir)) for path in bundle.elf_files(appdir / "usr")]


def smoke_one(name: str, layout: config.Layout, appimage: Path, env: Mapping[str, str]) -> None:
    tag = smoke_image(name, DISTROS[name], env)
    box = Container(tag, appimage, env)
    run = f"/appimage/{appimage.name}"
    try:
        box.run("mkdir -p /work /tmp/home /tmp/xdg && touch /tmp/marker")
        if "libfuse.so.2" in box.run("ldconfig -p").stdout:
            raise RuntimeError(f"{name}: libfuse2 is installed, so the check proves nothing")
        if "opensu" not in box.run(f"{run} --help").stdout:
            raise RuntimeError(f"{name}: `--help` through the FUSE mount printed no usage")
        box.run(f"cd /tmp && {run} --appimage-extract >/dev/null")
        for rel in bundled_elves(layout.appdir):
            listing = box.run(f"ldd /tmp/squashfs-root/{rel}").stdout
            if "not found" in listing:
                raise RuntimeError(f"{name}: {rel} has unresolved libraries:\n{listing}")
        gamescope = box.run(
            "LD_BIND_NOW=1 /tmp/squashfs-root/usr/libexec/opensu/gamescope --help", check=False
        )
        if "usage" not in (gamescope.stdout + gamescope.stderr).lower():
            raise RuntimeError(f"{name}: gamescope did not load:\n{gamescope.stderr}")
        box.run("rm -rf /tmp/squashfs-root")
        box.run(
            "cd /work && HOME=/tmp/home XDG_CONFIG_HOME=/tmp/xdg/config "
            "XDG_CACHE_HOME=/tmp/xdg/cache XDG_DATA_HOME=/tmp/xdg/data "
            f"{run} --render /tmp/frame.png"
        )
        frame = layout.smoke / f"{name}.png"
        box.copy_out("/tmp/frame.png", frame)
        if png_size(frame) != RENDER_SIZE:
            raise RuntimeError(f"{name}: the frame is {png_size(frame)}, not {RENDER_SIZE}")
        written = box.run(
            "find / -xdev -newer /tmp/marker -type f -not -path '/proc/*' -not -path '/sys/*'"
        ).stdout.split()
        stray = [path for path in written if not path.startswith(("/tmp/xdg/", "/tmp/frame.png"))]
        if stray:
            raise RuntimeError(f"{name}: the run wrote outside XDG: {stray}")
        print(f"{name}: ok ({len(written)} files under XDG, frame {frame})", flush=True)
    finally:
        box.remove()


def run_all(layout: config.Layout, version: str, names: Sequence[str]) -> None:
    appimage = layout.appimage(version)
    if not appimage.is_file():
        raise RuntimeError(f"{appimage} is missing; run `build` first")
    check_runtime(appimage)
    env = process.build_environment(layout.scratch)
    failures = []
    for name in names:
        try:
            smoke_one(name, layout, appimage, env)
        except RuntimeError as error:
            failures.append(f"{name}: {error}")
            print(f"{name}: FAILED: {error}", flush=True)
    if failures:
        raise RuntimeError("smoke failed on " + ", ".join(name.split(":")[0] for name in failures))
