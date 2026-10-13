"""A private headless KWin standing in for the desktop Gamescope nests in."""

import os
import shutil
import signal
import subprocess
import time
from pathlib import Path

SOCKET = "osu-remap"
START_TIMEOUT_S = 15.0
KWIN_SERVICE = "org.kde.KWin"


class HostCompositor:
    """`kwin_wayland --virtual` on its own runtime dir and session bus, stopped by its PID group."""

    def __init__(self, root: Path, width: int, height: int) -> None:
        missing = [t for t in ("kwin_wayland", "dbus-daemon", "gdbus") if shutil.which(t) is None]
        if missing:
            raise RuntimeError(
                f"{', '.join(missing)} needed: `sudo dnf install kwin dbus-daemon glib2`"
            )
        self.runtime = root / "rt"
        self.log = root / "kwin.log"
        self._width = width
        self._height = height
        self._bus: subprocess.Popen[bytes] | None = None
        self._kwin: subprocess.Popen[bytes] | None = None
        self._bus_address = ""

    def __enter__(self) -> "HostCompositor":
        self.runtime.mkdir(mode=0o700, parents=True)
        # Its own process group, so the bus, KWin and anything the bus activates stop together.
        self._bus = subprocess.Popen(
            ["dbus-daemon", "--session", "--nofork", "--print-address=1"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, process_group=0,
        )  # fmt: skip
        assert self._bus.stdout is not None
        self._bus_address = self._bus.stdout.readline().decode().strip()
        env = self._base_env()
        env.pop("WAYLAND_DISPLAY", None)
        command = [
            "kwin_wayland", "--virtual", "--no-lockscreen",
            "--width", str(self._width), "--height", str(self._height), "--socket", SOCKET,
        ]  # fmt: skip
        try:
            with self.log.open("wb") as log:
                self._kwin = subprocess.Popen(
                    command, env=env, stdout=log, stderr=subprocess.STDOUT,
                    process_group=self._bus.pid,
                )  # fmt: skip
            self._wait_for_socket()
        except BaseException:
            self.stop()
            raise
        return self

    def __exit__(self, *_exc: object) -> None:
        self.stop()

    def client_env(self) -> dict[str, str]:
        env = self._base_env()
        env["WAYLAND_DISPLAY"] = SOCKET
        return env

    def share_user_manager(self) -> None:
        """Lets clients reach the user's systemd manager, which owns openSU's scopes."""
        (self.runtime / "systemd").symlink_to(Path(os.environ["XDG_RUNTIME_DIR"]) / "systemd")

    def load_script(self, qml: Path, name: str) -> None:
        """Loads and runs a declarative KWin script."""
        # KWin takes its bus name a moment after it opens the Wayland socket.
        subprocess.run(["gdbus", "wait", "--session", "--timeout", str(int(START_TIMEOUT_S)),
                        KWIN_SERVICE], env=self._base_env(), check=True)  # fmt: skip
        reply = self._gdbus("/Scripting", "org.kde.kwin.Scripting.loadDeclarativeScript",
                            str(qml), name)  # fmt: skip
        script_id = int(reply.strip("()\n, ").split()[-1])
        if script_id < 0:
            raise RuntimeError(f"KWin refused the script {qml}")
        self._gdbus(f"/Scripting/Script{script_id}", "org.kde.kwin.Script.run")

    def stop(self) -> None:
        if self._bus is None:
            return
        group = self._bus.pid
        try:
            os.killpg(group, signal.SIGTERM)
            for process in (self._kwin, self._bus):
                if process is not None:
                    process.wait(timeout=10)
        except ProcessLookupError:
            pass
        except subprocess.TimeoutExpired:
            os.killpg(group, signal.SIGKILL)
            for process in (self._kwin, self._bus):
                if process is not None:
                    process.wait()
        self._kwin = None
        self._bus = None

    def _base_env(self) -> dict[str, str]:
        env = {k: v for k, v in os.environ.items() if k != "DISPLAY"}
        env["XDG_RUNTIME_DIR"] = str(self.runtime)
        env["DBUS_SESSION_BUS_ADDRESS"] = self._bus_address
        return env

    def _gdbus(self, path: str, method: str, *args: str) -> str:
        command = ["gdbus", "call", "--session", "--dest", KWIN_SERVICE,
                   "--object-path", path, "--method", method, *args]  # fmt: skip
        done = subprocess.run(command, env=self._base_env(), capture_output=True, text=True,
                              check=True)  # fmt: skip
        return done.stdout

    def _wait_for_socket(self) -> None:
        deadline = time.monotonic() + START_TIMEOUT_S
        socket = self.runtime / SOCKET
        while not socket.exists():
            if self._kwin is not None and self._kwin.poll() is not None:
                raise RuntimeError(f"kwin_wayland exited early, see {self.log}")
            if time.monotonic() > deadline:
                raise RuntimeError(f"kwin_wayland did not open its socket, see {self.log}")
            time.sleep(0.1)
