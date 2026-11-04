"""The Linux build image (packaging/ci): every Linux step runs in it, so the host needs podman and
none of openSU's build tools or dev packages.

A maintainer's deps, configure, build and lint run in the image built on the host's own Fedora
release, with the checkout, its git common dir and the shared third-party prefix mounted at their
own paths, so the tree configured inside is the same tree outside and its binaries link against
libraries the host has. The tests run on the host, where the product runs.

CI runs `all` in the image built on the pinned Fedora 44. The tests start programs in systemd user
scopes, so there the image boots systemd, the user manager is started for this user's uid, and the
steps run as that user (a root run would pass permission checks the tests expect to fail).
"""

import os
import shutil
import subprocess
from collections.abc import Mapping, Sequence
from pathlib import Path

from tools import podman
from tools.check import process, thirdparty
from tools.checkout import REPO

CONTAINERFILE = REPO / "packaging" / "ci" / "Containerfile"
IMAGE_NAME = "localhost/opensu-build"
# The steps that need the build tools; the tests run where the product runs.
IN_IMAGE = ("deps", "configure", "build", "lint", "install")
OS_RELEASE = Path("/etc/os-release")
# podman's own marker in every container it runs.
CONTAINER_MARKER = Path("/run/.containerenv")
PODMAN_INSTALL = (
    "Fedora: `sudo dnf install podman`; Debian and Ubuntu: `sudo apt install podman`; Arch: "
    "`sudo pacman -S podman`; openSUSE: `sudo zypper install podman`"
)


def require_podman(env: Mapping[str, str]) -> None:
    """Refuse a host whose podman is missing or cannot run rootless containers."""
    if shutil.which("podman", path=env.get("PATH")) is None:
        raise SystemExit(
            f"openSU builds on Linux in a podman container; install podman. {PODMAN_INSTALL}"
        )
    info = subprocess.run(
        ["podman", "info"], env=dict(env), capture_output=True, text=True, check=False
    )
    if info.returncode != 0:
        raise SystemExit(
            f"openSU builds on Linux in a podman container, and `podman info` fails here:\n"
            f"{info.stderr.strip()}\nRootless podman needs the user's subordinate ids "
            f"(/etc/subuid, /etc/subgid). {PODMAN_INSTALL}"
        )


def require_inside() -> None:
    """Refuse `--in-build-image` outside a container, where the host's tools would build."""
    if not CONTAINER_MARKER.is_file():
        raise SystemExit("--in-build-image runs the steps in place, which only the build image may")


def host_base(os_release: Path = OS_RELEASE) -> str:
    """The Fedora image of the host's own release; any other host is refused, since the image's
    binaries need a Fedora host's libraries."""
    fields = {}
    for line in os_release.read_text().splitlines():
        key, sep, value = line.partition("=")
        if sep:
            fields[key] = value.strip().strip('"')
    if fields.get("ID") != "fedora" or not fields.get("VERSION_ID"):
        raise SystemExit(
            f"the build image is Fedora's, whose binaries need a Fedora host; on "
            f"{fields.get('PRETTY_NAME', 'this host')} build by hand with the distribution's "
            "packages (docs/guide/building.md, By hand)"
        )
    return f"registry.fedoraproject.org/fedora:{fields['VERSION_ID']}"


def ensure_image(env: Mapping[str, str], base: str | None = None) -> str:
    """The image on `base`, or on the Containerfile's pinned Fedora when None."""
    build_args = () if base is None else (f"BASE={base}",)
    tag = podman.content_tag(IMAGE_NAME, CONTAINERFILE.read_bytes(), *build_args)
    return podman.ensure_image(tag, env, containerfile=CONTAINERFILE, build_args=build_args)


def _tree_mounts(build_dir: Path) -> list[str]:
    """The checkout, its git common dir and the build directory, each at its own path."""
    words = ["-v", f"{REPO}:{REPO}"]
    common = podman.git_common_dir(REPO)
    if common is not None:
        words += ["-v", f"{common}:{common}:ro"]
    if not build_dir.is_relative_to(REPO):
        words += ["-v", f"{build_dir}:{build_dir}"]
    return words


def run_step(
    step: str,
    build_dir: Path,
    options: Sequence[str],
    writes: Sequence[Path],
    env: Mapping[str, str],
) -> None:
    """`tools.check <step> <options>` in the image of the host's release, as the calling user, with
    `writes` (an install's target) mounted at their own paths too; HOME is the host's path so the
    shared third-party prefix is the one outside, and nothing is written under it."""
    tag = ensure_image(env, host_base())
    shared = thirdparty.DEFAULT_ROOT
    build_dir.mkdir(parents=True, exist_ok=True)
    mounts = []
    for path in (shared, *writes):
        path.mkdir(parents=True, exist_ok=True)
        mounts += ["-v", f"{path}:{path}"]
    words: list[str | Path] = [
        "podman", "run", "--rm", "--userns=keep-id", "--security-opt", "label=disable",
        "-w", str(REPO), *_tree_mounts(build_dir), *mounts,
        "-e", f"HOME={Path.home()}",
        "-e", f"CARGO_HOME={build_dir / 'cargo'}",
        "-e", f"UV_CACHE_DIR={build_dir / 'uv-cache'}",
        "-e", f"UV_PROJECT_ENVIRONMENT={build_dir / 'venv'}",
        tag, "uv", "run", "--frozen", "python", "-m", "tools.check", step, "--in-build-image",
        "--build-dir", str(build_dir), *options,
    ]  # fmt: skip
    process.run(words, env)


def run_all(build_dir: Path, jobs: int, env: Mapping[str, str]) -> None:
    """`tools.check all --gamescope native` in the image on the pinned Fedora, against this
    checkout, on the caller's display for the tests' hidden windows."""
    tag = ensure_image(env)
    uid = os.getuid()
    words: list[str | Path] = [
        "podman", "run", "-d", "--rm", "--systemd=always", "--userns=keep-id", "--user", "root",
        "--security-opt", "label=disable", *_tree_mounts(build_dir), *podman.display_words(env),
    ]  # fmt: skip
    container = process.output([*words, tag], env).strip()
    try:
        _exec(container, "root", ["systemctl", "is-system-running", "--wait", "--quiet"], env,
              allowed=(0, 1))  # fmt: skip
        _exec(container, "root", ["systemctl", "start", f"user@{uid}.service"], env)
        runtime = f"/run/user/{uid}"
        home = build_dir / "home"
        home.mkdir(parents=True, exist_ok=True)
        _exec(
            container,
            str(uid),
            [
                "env", f"HOME={home}", f"XDG_RUNTIME_DIR={runtime}",
                f"DBUS_SESSION_BUS_ADDRESS=unix:path={runtime}/bus",
                f"UV_CACHE_DIR={build_dir / 'uv-cache'}",
                f"UV_PROJECT_ENVIRONMENT={build_dir / 'venv'}",
                "uv", "run", "--frozen", "python", "-m", "tools.check", "all", "--in-build-image",
                "--gamescope", "native", "--build-dir", str(build_dir), "--jobs", str(jobs),
            ],
            env,
        )  # fmt: skip
    finally:
        process.run(["podman", "rm", "-f", "-t", "0", container], env)


def _exec(
    container: str,
    user: str,
    command: Sequence[str | Path],
    env: Mapping[str, str],
    allowed: Sequence[int] = (0,),
) -> None:
    """Run `command` in the container as `user`; an exit status outside `allowed` fails."""
    words = ["podman", "exec", "--user", user, "-w", str(REPO), container, *map(str, command)]
    print("+ " + " ".join(words), flush=True)
    status = subprocess.run(words, env=dict(env), check=False).returncode
    if status not in allowed:
        raise RuntimeError(f"`{' '.join(map(str, command))}` exited {status}")
