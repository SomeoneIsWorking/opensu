"""One hidden openSU run, driven over its loopback control channel."""

import json
import os
import re
import subprocess
import time
import urllib.request
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

PORT_LINE = re.compile(r"control channel on http://127\.0\.0\.1:(\d+)/")

# Every integration on, chosen ahead so the first-run chooser stays away and every store shows.
INTEGRATIONS = {
    "integrations": {
        "steam": True,
        "steamMode": "managed",
        "epic": True,
        "gog": True,
        "emulators": True,
        "plugins": True,
    }
}


@dataclass(frozen=True)
class Profile:
    """A throwaway home: every directory openSU reads or writes for the player lives under it."""

    home: Path
    steam_root: Path
    rom_root: Path
    # A directory put first on PATH, for host tools stood in by fixtures.
    tools: Path | None = None

    def environment(self) -> dict[str, str]:
        env = {
            key: value
            for key, value in os.environ.items()
            if key in ("DISPLAY", "XAUTHORITY", "PATH", "LANG", "LC_ALL", "LC_TIME")
        }
        env.update(
            HOME=str(self.home),
            XDG_CONFIG_HOME=str(self.home / ".config"),
            XDG_CACHE_HOME=str(self.home / ".cache"),
            XDG_DATA_HOME=str(self.home / ".local" / "share"),
            XDG_RUNTIME_DIR=str(self.home / "run"),
            OPENSU_STEAM_ROOTS=str(self.steam_root),
            OPENSU_ROM_ROOTS=str(self.rom_root),
            LC_TIME="C.UTF-8",
        )
        if self.tools is not None:
            env["PATH"] = os.pathsep.join(filter(None, (str(self.tools), env.get("PATH"))))
        return env


class HiddenShell:
    """openSU --hidden; stopped through /quit, else by the PID it was started with."""

    def __init__(self, executable: Path, profile: Profile, log: Path) -> None:
        self._executable = executable
        self._profile = profile
        self._log = log
        self._process: subprocess.Popen[bytes] | None = None
        self._base = ""

    def __enter__(self) -> "HiddenShell":
        (self._profile.home / "run").mkdir(parents=True, exist_ok=True, mode=0o700)
        settings = self._profile.home / ".config" / "opensu" / "settings.json"
        if not settings.exists():
            settings.parent.mkdir(parents=True, exist_ok=True)
            settings.write_text(json.dumps(INTEGRATIONS))
        self._log.parent.mkdir(parents=True, exist_ok=True)
        with self._log.open("wb") as log:
            self._process = subprocess.Popen(
                [str(self._executable), "--hidden"],
                env=self._profile.environment(),
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        self._base = f"http://127.0.0.1:{self._wait_port()}"
        return self

    def __exit__(self, *_exc: object) -> None:
        process = self._process
        if process is None or process.poll() is not None:
            return
        try:
            self.post("/quit", "")
            process.wait(timeout=10)
        except (OSError, subprocess.TimeoutExpired):
            process.kill()
            process.wait(timeout=10)

    def _wait_port(self, timeout: float = 30.0) -> int:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self._process is not None and self._process.poll() is not None:
                status = self._process.returncode
                raise RuntimeError(f"openSU exited with {status}; see {self._log}")
            found = PORT_LINE.search(self._log.read_text(errors="replace"))
            if found:
                return int(found.group(1))
            time.sleep(0.2)
        raise RuntimeError(f"openSU logged no control port in {timeout}s; see {self._log}")

    def _request(self, path: str, body: str | None) -> bytes:
        data = None if body is None else body.encode()
        request = urllib.request.Request(self._base + path, data=data)
        with urllib.request.urlopen(request, timeout=30) as response:
            return response.read()

    def post(self, path: str, body: str) -> None:
        self._request(path, body)

    def state(self) -> dict[str, object]:
        return json.loads(self._request("/state", None))

    def frame(self) -> bytes:
        return self._request("/frame.png", None)

    def press(self, *buttons: str, settle: float = 0.35) -> None:
        """Taps each button in turn; `guide+a` holds Guide while A is tapped."""
        for button in buttons:
            self.post("/input", button)
            time.sleep(settle)

    def key(self, combo: str, settle: float = 0.35) -> None:
        self.post("/key", combo)
        time.sleep(settle)

    def type_text(self, text: str) -> None:
        self.post("/text", text)

    def wait_until(
        self, holds: Callable[[dict[str, object]], bool], what: str, timeout: float = 20.0
    ) -> dict[str, object]:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = self.state()
            if holds(state):
                return state
            time.sleep(0.05)
        raise RuntimeError(f"state never showed {what}: {self.state()}")

    def wait_for(self, field: str, value: object, timeout: float = 20.0) -> dict[str, object]:
        return self.wait_until(
            lambda state: state.get(field) == value, f"{field}={value!r}", timeout
        )
