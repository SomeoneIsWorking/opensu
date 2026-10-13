"""Running build commands with temporary files kept inside the build directory."""

import os
import shlex
import subprocess
from collections.abc import Mapping, Sequence
from pathlib import Path


def build_environment(scratch: Path, extra: Mapping[str, str] | None = None) -> dict[str, str]:
    """The current environment with TMPDIR and CCACHE_TEMPDIR under `scratch`."""
    scratch.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["TMPDIR"] = str(scratch)
    env["CCACHE_TEMPDIR"] = str(scratch)
    if extra:
        env.update(extra)
    return env


def run(command: Sequence[str | Path], env: Mapping[str, str], cwd: Path | None = None) -> None:
    """Run `command`, echoing it; a non-zero exit raises CalledProcessError."""
    words = [str(word) for word in command]
    print("+ " + shlex.join(words), flush=True)
    subprocess.run(words, env=dict(env), cwd=cwd, check=True)


def output(command: Sequence[str | Path], env: Mapping[str, str], cwd: Path | None = None) -> str:
    """The standard output of `command`; a non-zero exit raises CalledProcessError."""
    words = [str(word) for word in command]
    return subprocess.run(
        words, env=dict(env), cwd=cwd, check=True, capture_output=True, text=True
    ).stdout


def run_input(command: Sequence[str | Path], text: str, env: Mapping[str, str]) -> None:
    """Run `command` with `text` on its standard input; the text is never echoed."""
    words = [str(word) for word in command]
    print("+ " + shlex.join(words) + " < (stdin)", flush=True)
    subprocess.run(words, env=dict(env), input=text, text=True, check=True)
