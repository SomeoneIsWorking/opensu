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
SECONDS_PER_CYCLE = 3.0
# Gamescope's shutdown once its host display is gone.
HOST_LOSS_WAIT_S = 20.0
FOCUS_FLAP = Path(__file__).resolve().parent / "kwin" / "focus_flap.qml"


@dataclass(frozen=True)
class RunSpec:
    gamescope: Path
    width: int
    height: int
    cycles: int
    scenario: str = "cycle"
    seed: int = 0
    focus_flap: bool = False
    host_exit_after: float | None = None


def gamescope_command(spec: RunSpec) -> list[str]:
    """openSU's nested command line (src/session/gamescope.cpp) with the window-cycle child."""
    size = [str(spec.width), str(spec.height)]
    child = [sys.executable, "-m", "tools.nested_remap.child", spec.scenario, str(spec.cycles),
             str(spec.seed)]  # fmt: skip
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
            code = -int(signal.SIGKILL)
            try:
                if spec.focus_flap:
                    host.load_script(FOCUS_FLAP, "focus_flap")
                if spec.host_exit_after is None:
                    code = process.wait(timeout=SLACK_S + spec.cycles * SECONDS_PER_CYCLE)
                else:
                    code = _wait_through_host_loss(process, host, spec.host_exit_after)
            except subprocess.TimeoutExpired:
                pass
            finally:
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
    return judge(code, log_path.read_text(errors="replace"), spec.host_exit_after is not None)


def _wait_through_host_loss(
    process: subprocess.Popen[bytes], host: HostCompositor, after_s: float
) -> int:
    """Stops the host after `after_s` unless Gamescope ended first, then waits for Gamescope."""
    try:
        return process.wait(timeout=after_s)
    except subprocess.TimeoutExpired:
        host.stop()
    return process.wait(timeout=HOST_LOSS_WAIT_S)


def _no_core_dump() -> None:
    # The crash under test aborts; its core dump tells nothing the log does not.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
