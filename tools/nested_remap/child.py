"""The program Gamescope runs: X windows that close and open, like a game quitting back to its
launcher. Run as `python -m tools.nested_remap.child SCENARIO CYCLES SEED`.

Scenarios:
  cycle  one window at a time, closed for half a second between them
  rapid  short-lived windows with short or no gaps, often closing before their first frame
  steam  overlapping windows handing the focus to each other with brief gaps, like Steam's start
"""

import random
import shutil
import subprocess
import sys
import time
from collections.abc import Callable

WINDOW_S = 1.5
GAP_S = 0.5


class Windows:
    """glxgears windows by name; every one still open is closed at the end."""

    def __init__(self) -> None:
        self._open: dict[str, subprocess.Popen[bytes]] = {}

    def open(self, name: str) -> None:
        quiet = {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
        self._open[name] = subprocess.Popen(["glxgears"], **quiet)

    def close(self, name: str) -> None:
        window = self._open.pop(name)
        window.terminate()
        window.wait()

    def close_all(self) -> None:
        for name in list(self._open):
            self.close(name)


def cycle(windows: Windows, _rng: random.Random) -> None:
    windows.open("game")
    time.sleep(WINDOW_S)
    windows.close("game")
    # With no window left Gamescope hides its toplevel; the next window shows it again.
    time.sleep(GAP_S)


def rapid(windows: Windows, rng: random.Random) -> None:
    windows.open("game")
    time.sleep(rng.uniform(0.02, 0.4))
    windows.close("game")
    time.sleep(rng.choice((0.0, 0.01, 0.05, 0.2)))


def steam(windows: Windows, rng: random.Random) -> None:
    windows.open("splash")
    time.sleep(rng.uniform(0.2, 0.5))
    windows.open("login")
    time.sleep(0.1)
    windows.close("splash")
    time.sleep(rng.uniform(0.2, 0.4))
    windows.close("login")
    time.sleep(rng.choice((0.0, 0.02, 0.1)))
    windows.open("main")
    windows.open("update")
    time.sleep(rng.uniform(0.05, 0.3))
    windows.close("update")
    time.sleep(rng.uniform(0.3, 0.6))
    windows.close("main")
    time.sleep(rng.choice((0.02, 0.2)))


SCENARIOS: dict[str, Callable[[Windows, random.Random], None]] = {
    "cycle": cycle,
    "rapid": rapid,
    "steam": steam,
}


def main(scenario: str, cycles: int, seed: int) -> int:
    if shutil.which("glxgears") is None:
        sys.stderr.write("glxgears is needed: `sudo dnf install glx-utils`\n")
        return 2
    step = SCENARIOS[scenario]
    rng = random.Random(seed)
    windows = Windows()
    try:
        for _ in range(cycles):
            step(windows, rng)
    finally:
        windows.close_all()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3])))
