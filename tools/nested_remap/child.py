"""The program Gamescope runs: an X window that closes and a new one that opens, like a game
quitting back to its launcher. Run as `python -m tools.nested_remap.child CYCLES`."""

import shutil
import subprocess
import sys
import time

WINDOW_S = 1.5
GAP_S = 0.5


def main(cycles: int) -> int:
    if shutil.which("glxgears") is None:
        sys.stderr.write("glxgears is needed: `sudo dnf install glx-utils`\n")
        return 2
    for _ in range(cycles):
        quiet = {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
        window = subprocess.Popen(["glxgears"], **quiet)
        time.sleep(WINDOW_S)
        window.terminate()
        window.wait()
        # With no window left Gamescope hides its toplevel; the next window shows it again.
        time.sleep(GAP_S)
    return 0


if __name__ == "__main__":
    sys.exit(main(int(sys.argv[1])))
