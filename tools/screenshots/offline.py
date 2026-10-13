"""Runs the capture in a network namespace of its own, so openSU can download nothing."""

import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

MARKER = "OPENSU_SCREENSHOTS_OFFLINE"


def is_offline() -> bool:
    return os.environ.get(MARKER) == "1"


def reexec_offline(argv: list[str]) -> int:
    """This command again, in a new user and network namespace with only loopback up."""
    env = dict(os.environ, **{MARKER: "1"})
    command = [
        "unshare",
        "--user",
        "--map-current-user",
        "--keep-caps",
        "--net",
        "--",
        "sh",
        "-c",
        'ip link set lo up && exec "$@"',
        "sh",
        sys.executable,
        "-m",
        "tools.screenshots",
        *argv,
    ]
    return subprocess.run(command, env=env, cwd=REPO, check=False).returncode
