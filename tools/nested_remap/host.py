"""A private headless KWin standing in for the desktop Gamescope nests in."""

import os
import shutil
import signal
import subprocess
import time
from pathlib import Path

SOCKET = "osu-remap"
START_TIMEOUT_S = 15.0


class HostCompositor:
    """`kwin_wayland --virtual` on its own runtime dir and session bus, stopped by its PID group."""

    def __init__(self, root: Path, width: int, height: int) -> None:
        if shutil.which("kwin_wayland") is None or shutil.which("dbus-run-session") is None:
            raise RuntimeError(
                "kwin_wayland and dbus-run-session are needed: `sudo dnf install kwin dbus-daemon`"
            )
        self.runtime = root / "rt"
        self.log = root / "kwin.log"
        self._width = width
        self._height = height
        self._process: subprocess.Popen[bytes] | None = None

    def __enter__(self) -> "HostCompositor":
        self.runtime.mkdir(mode=0o700, parents=True)
        env = {k: v for k, v in os.environ.items() if k not in ("DISPLAY", "WAYLAND_DISPLAY")}
        env["XDG_RUNTIME_DIR"] = str(self.runtime)
        command = [
            "dbus-run-session", "--", "kwin_wayland", "--virtual", "--no-lockscreen",
            "--width", str(self._width), "--height", str(self._height), "--socket", SOCKET,
        ]  # fmt: skip
        with self.log.open("wb") as log:
            # Its own process group, so the bus, KWin and anything the bus activates stop together.
            self._process = subprocess.Popen(
                command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True
            )
        self._wait_for_socket()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.stop()

    def client_env(self) -> dict[str, str]:
        env = {k: v for k, v in os.environ.items() if k != "DISPLAY"}
        env["XDG_RUNTIME_DIR"] = str(self.runtime)
        env["WAYLAND_DISPLAY"] = SOCKET
        return env

    def stop(self) -> None:
        if self._process is None:
            return
        group = self._process.pid
        try:
            os.killpg(group, signal.SIGTERM)
            self._process.wait(timeout=10)
        except ProcessLookupError:
            pass
        except subprocess.TimeoutExpired:
            os.killpg(group, signal.SIGKILL)
            self._process.wait()
        self._process = None

    def _wait_for_socket(self) -> None:
        deadline = time.monotonic() + START_TIMEOUT_S
        socket = self.runtime / SOCKET
        while not socket.exists():
            if self._process is not None and self._process.poll() is not None:
                raise RuntimeError(f"kwin_wayland exited early, see {self.log}")
            if time.monotonic() > deadline:
                self.stop()
                raise RuntimeError(f"kwin_wayland did not open its socket, see {self.log}")
            time.sleep(0.1)
