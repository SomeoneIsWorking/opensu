"""Check that openSU's nested session comes back after Gamescope crashes, and gives up past the
restart limit: `uv run --frozen python -m tools.nested_remap.restart`.

The real CompositorSession and the real Gamescope run inside the private headless KWin, with
tests/session/nested_session_driver standing in for the shell (the shell would grab the
machine's pads). Once each Gamescope's child is up, Gamescope is killed with SIGABRT by the PID
read from the session's compositor scope, once more than the limit allows."""

import argparse
import os
import resource
import signal
import subprocess
import time
from datetime import datetime
from pathlib import Path

from tools.nested_remap.host import HostCompositor

REPO = Path(__file__).resolve().parents[2]
BUILD = REPO / "build" / "clang"
# CompositorSession's default limit.
RESTARTS = 3
START_TIMEOUT_S = 30.0
END_TIMEOUT_S = 60.0
CHILD_UP = "child up in gamescope"


def scope_pids(unit: str) -> list[int]:
    """The processes in a user scope, from its cgroup; none when the scope is not up."""
    done = subprocess.run(
        ["systemctl", "--user", "show", "-P", "ControlGroup", unit],
        capture_output=True, text=True, check=False,
    )  # fmt: skip
    group = done.stdout.strip()
    if not group:
        return []
    try:
        procs = Path("/sys/fs/cgroup" + group, "cgroup.procs").read_text()
    except FileNotFoundError:
        return []
    return [int(line) for line in procs.split()]


def comm(pid: int) -> str:
    try:
        return Path(f"/proc/{pid}/comm").read_text().strip()
    except FileNotFoundError:
        return ""


def wait_for_gamescope(
    unit: str, driver: subprocess.Popen[bytes], previous: int | None
) -> int | None:
    """A Gamescope in `unit` other than `previous`, or None once the session has ended."""
    deadline = time.monotonic() + START_TIMEOUT_S
    while time.monotonic() < deadline:
        for pid in scope_pids(unit):
            if pid != previous and comm(pid) == "gamescope":
                return pid
        if driver.poll() is not None:
            return None
        time.sleep(0.1)
    return None


def wait_for_child(log_path: Path, starts: int) -> bool:
    """Whether the stand-in shell of the `starts`th Gamescope came up."""
    deadline = time.monotonic() + START_TIMEOUT_S
    while time.monotonic() < deadline:
        if log_path.read_text(errors="replace").count(CHILD_UP) >= starts:
            return True
        time.sleep(0.1)
    return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="nested_remap.restart", description=__doc__)
    parser.add_argument(
        "--driver", type=Path, default=BUILD / "tests" / "session" / "nested_session_driver"
    )
    parser.add_argument(
        "--gamescope", type=Path, default=BUILD / "libexec" / "opensu" / "gamescope"
    )
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args(argv)

    root = REPO / "scratch" / "nested-remap" / datetime.now().strftime("restart-%Y%m%d-%H%M%S")
    root.mkdir(parents=True)
    session = f"opensu-restart-check-{os.getpid()}"
    unit = f"{session}-compositor.scope"
    log_path = root / "session.log"
    with HostCompositor(root, args.width, args.height) as host:
        host.share_user_manager()
        command = [str(args.driver.resolve()), session, str(args.gamescope.resolve()),
                   str(root / "desktop"), str(args.width), str(args.height)]  # fmt: skip
        with log_path.open("wb") as log:
            driver = subprocess.Popen(
                command, env=host.client_env(), stdout=log, stderr=subprocess.STDOUT,
                start_new_session=True, preexec_fn=_no_core_dump,
            )  # fmt: skip
        killed: list[int] = []
        try:
            pid = wait_for_gamescope(unit, driver, None)
            while pid is not None and wait_for_child(log_path, len(killed) + 1):
                os.kill(pid, signal.SIGABRT)
                killed.append(pid)
                print(f"killed gamescope pid {pid} with SIGABRT", flush=True)
                pid = wait_for_gamescope(unit, driver, pid)
                if pid is not None:
                    print(f"the session came back: gamescope pid {pid}", flush=True)
            if pid is not None:
                print("a Gamescope's child never came up", flush=True)
            code = driver.wait(timeout=END_TIMEOUT_S)
        finally:
            if driver.poll() is None:
                driver.send_signal(signal.SIGTERM)
                driver.wait(timeout=END_TIMEOUT_S)
    log_text = log_path.read_text(errors="replace")
    restarts = log_text.count("starting a fresh")
    gave_up = "giving up" in log_text
    print(f"{len(killed)} kills, {restarts} restarts, gave up: {gave_up}, session exit {code}, "
          f"log {log_path}")  # fmt: skip
    passed = len(killed) == RESTARTS + 1 and restarts == RESTARTS and gave_up
    return 0 if passed and code == 128 + signal.SIGABRT else 1


def _no_core_dump() -> None:
    # The kills are deliberate; their core dumps tell nothing.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


if __name__ == "__main__":
    raise SystemExit(main())
