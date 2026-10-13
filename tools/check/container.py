"""The CI environment: `all` inside the Fedora 44 image of packaging/ci, with systemd as PID 1.

The tests start programs in systemd user scopes, so a plain container is not enough: the image boots
systemd, the user manager is started for this user's uid, and the steps run as that user (a root
run would pass permission checks the tests expect to fail).
"""

import os
import subprocess
from collections.abc import Mapping, Sequence
from pathlib import Path

from tools import podman
from tools.check import process
from tools.check.cmake import REPO

CONTAINERFILE = REPO / "packaging" / "ci" / "Containerfile"
IMAGE_NAME = "localhost/opensu-ci"


def ensure_image(env: Mapping[str, str]) -> str:
    tag = podman.content_tag(IMAGE_NAME, CONTAINERFILE.read_bytes())
    return podman.ensure_image(tag, env, containerfile=CONTAINERFILE)


def run_all(build_dir: Path, env: Mapping[str, str]) -> None:
    """`tools.check all --gamescope native` in the CI image, against this checkout."""
    tag = ensure_image(env)
    uid = os.getuid()
    words: list[str | Path] = [
        "podman", "run", "-d", "--rm", "--systemd=always", "--userns=keep-id", "--user", "root",
        "--security-opt", "label=disable", "-v", f"{REPO}:{REPO}",
    ]  # fmt: skip
    common = podman.git_common_dir(REPO)
    if common is not None:
        words += ["-v", f"{common}:{common}:ro"]
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
                "uv", "run", "--frozen", "python", "-m", "tools.check", "all",
                "--gamescope", "native", "--build-dir", str(build_dir),
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
