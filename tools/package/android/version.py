"""The APK's version from `git describe`: v0.2.0-15-gabc is name 0.2.0-15-gabc, code 200015."""

import re
import subprocess

from tools.package.android import config


def describe() -> str:
    return subprocess.run(
        ["git", "-C", str(config.REPO), "describe", "--tags", "--always", "--dirty"],
        capture_output=True, text=True, check=True,
    ).stdout.strip().removeprefix("v")  # fmt: skip


def code(name: str) -> int:
    """Each release tag a thousand codes apart, each commit after it one more."""
    match = re.match(r"^(\d+)\.(\d+)\.(\d+)(?:-(\d+)-g[0-9a-f]+)?", name)
    if match is None:
        raise config.refuse(f"the version {name!r} is not MAJOR.MINOR.PATCH[-N-gHASH]")
    major, minor, patch = (int(part) for part in match.group(1, 2, 3))
    after = int(match.group(4) or 0)
    if after >= 1000 or minor >= 100 or patch >= 100:
        raise config.refuse(f"the version {name!r} does not fit the version code")
    return ((major * 100 + minor) * 100 + patch) * 1000 + after
