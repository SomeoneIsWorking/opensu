"""One nested Gamescope run inside the private host compositor."""

import os
import resource
import signal
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.nested_remap.host import HostCompositor
from tools.nested_remap.verdict import Verdict, judge

# The window cycles plus Gamescope's start and stop.
SLACK_S = 30.0
SECONDS_PER_CYCLE = 2.5


@dataclass(frozen=True)
class RunSpec:
    gamescope: Path
    width: int
    height: int
    cycles: int


def gamescope_command(spec: RunSpec) -> list[str]:
    """openSU's nested command line (src/session/gamescope.cpp) with the window-cycle child."""
    size = [str(spec.width), str(spec.height)]
    child = [sys.executable, "-m", "tools.nested_remap.child", str(spec.cycles)]
    return [
        str(spec.gamescope), "--backend", "wayland",
        "-W", size[0], "-H", size[1], "-w", size[0], "-h", size[1],
        "-r", "60", "-f", "--close-focused-window", "--", *child,
    ]  # fmt: skip


def run_once(spec: RunSpec, root: Path) -> Verdict:
    root.mkdir(parents=True)
    log_path = root / "gamescope.log"
    with HostCompositor(root, spec.width, spec.height) as host:
        env = host.client_env()
        env["WAYLAND_DEBUG"] = "client"
        env["PYTHONPATH"] = str(Path(__file__).resolve().parents[2])
        with log_path.open("wb") as log:
            process = subprocess.Popen(
                gamescope_command(spec), env=env, stdout=log, stderr=subprocess.STDOUT,
                start_new_session=True, preexec_fn=_no_core_dump,
            )  # fmt: skip
            try:
                code = process.wait(timeout=SLACK_S + spec.cycles * SECONDS_PER_CYCLE)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                code = -int(signal.SIGKILL)
    return judge(code, log_path.read_text(errors="replace"))


def _no_core_dump() -> None:
    # The crash under test aborts; its core dump tells nothing the log does not.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
