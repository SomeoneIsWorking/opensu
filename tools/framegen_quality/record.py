"""A scene's frames at the full rate: the built-in runner hidden, with a home of its own, writing
every frame it runs (`--frame-pngs`), the scene's presses sent over its control channel."""

import json
import os
import shutil
import subprocess
import threading
import time
import urllib.request
from pathlib import Path

from tools.framegen_quality.scenes import Scene

WIDTH = 1280
HEIGHT = 720


def _environment(home: Path, control: Path) -> dict[str, str]:
    keep = {
        k: os.environ[k]
        for k in ("DISPLAY", "XAUTHORITY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "PATH")
        if k in os.environ
    }
    return {
        **keep,
        "HOME": str(home),
        "XDG_CONFIG_HOME": str(home / ".config"),
        "XDG_DATA_HOME": str(home / ".local" / "share"),
        "XDG_CACHE_HOME": str(home / ".cache"),
        "XDG_STATE_HOME": str(home / ".local" / "state"),
        "DISABLE_LSFG": "1",
        "OPENSU_WIDTH": str(WIDTH),
        "OPENSU_HEIGHT": str(HEIGHT),
        "OPENSU_RETRO_CONTROL": str(control),
    }


class _Presser(threading.Thread):
    """Holds each press's buttons from its frame on, read from the runner's `/status`."""

    def __init__(self, scene: Scene, control: Path, process: subprocess.Popen[bytes]) -> None:
        super().__init__(daemon=True)
        self.scene = scene
        self.control = control
        self.process = process

    def _call(self, port: int, path: str, body: str | None = None) -> bytes:
        data = None if body is None else body.encode()
        request = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=data)
        with urllib.request.urlopen(request, timeout=5) as answer:
            return answer.read()

    def run(self) -> None:
        port = 0
        while self.process.poll() is None and not port:
            try:
                port = int(json.loads(self.control.read_text())["port"])
            except (OSError, ValueError, KeyError):
                time.sleep(0.05)
        events = []
        for press in self.scene.presses:
            events.append((press.at, json.dumps({"buttons": list(press.buttons)})))
            events.append((press.at + press.frames, "{}"))
        events.sort(key=lambda event: event[0])
        while events and self.process.poll() is None:
            try:
                frame = int(json.loads(self._call(port, "/status"))["frame"])
            except OSError:
                time.sleep(0.02)
                continue
            while events and frame >= events[0][0]:
                self._call(port, "/input", events.pop(0)[1])
            time.sleep(0.005)


def record(scene: Scene, runner: Path, into: Path) -> Path:
    """The scene's frames in `into/frames`, numbered from 0; the runner's log beside them."""
    if into.exists():
        shutil.rmtree(into)
    raw = into / "raw"
    raw.mkdir(parents=True)
    home = into / "home"
    home.mkdir()
    control = into / "control.json"
    total = scene.skip + scene.frames
    command = [str(runner), "--hidden", "--core", str(scene.core), "--saves", str(home / "saves")]
    for option in scene.options:
        command += ["--option", option]
    command += ["--frames", str(total), "--frame-pngs", str(raw), str(scene.rom)]
    with (into / "runner.log").open("wb") as log:
        process = subprocess.Popen(
            command, env=_environment(home, control), stdout=log, stderr=subprocess.STDOUT
        )
        presser = _Presser(scene, control, process)
        presser.start()
        code = process.wait()
        presser.join(timeout=5)
    if code != 0:
        raise SystemExit(f"{scene.name}: the runner exited {code}, see {into / 'runner.log'}")
    frames = into / "frames"
    frames.mkdir()
    for number in range(scene.frames):
        source = raw / f"{scene.skip + 1 + number:06}.png"
        if not source.is_file():
            raise SystemExit(f"{scene.name}: the runner wrote no {source.name}")
        source.rename(frames / f"{number:06}.png")
    shutil.rmtree(raw)
    return frames
