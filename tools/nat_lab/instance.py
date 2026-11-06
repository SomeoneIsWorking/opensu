"""One hidden openSU in a namespace, driven over its control channel from inside that namespace."""

import json
import os
import re
import shutil
import signal
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path

from tools.nat_lab.topology import Node


@dataclass
class Instance:
    """A hidden shell, its files all under `home`, never the player's."""

    name: str
    node: Node
    home: Path
    binary: Path
    roms: Path
    stun: str
    process: subprocess.Popen[bytes] | None = None
    port: int = 0
    ports_before: set[str] = field(default_factory=set)
    log: Path = field(init=False)

    def __post_init__(self) -> None:
        self.log = self.home / "shell.log"

    def install_core(self, core: Path) -> None:
        """Puts a downloaded core where this openSU's own downloads are."""
        cores = self.home / ".local" / "share" / "opensu" / "cores"
        cores.mkdir(parents=True, exist_ok=True)
        for each in (core, core.with_suffix(".json")):
            shutil.copy(each, cores)

    def start(self) -> None:
        self.home.mkdir(parents=True, exist_ok=True)
        config = self.home / ".config" / "opensu"
        config.mkdir(parents=True, exist_ok=True)
        (config / "settings.json").write_text(json.dumps({"onlineStunServers": self.stun}))
        env = {
            "HOME": str(self.home),
            "XDG_CONFIG_HOME": str(self.home / ".config"),
            "XDG_DATA_HOME": str(self.home / ".local" / "share"),
            "XDG_CACHE_HOME": str(self.home / ".cache"),
            "XDG_STATE_HOME": str(self.home / ".local" / "state"),
            "DISABLE_LSFG": "1",
            "OPENSU_ROM_ROOTS": str(self.roms),
            "OPENSU_SECRET_STORE": "file",
        }
        command = ["env", *[f"{key}={value}" for key, value in env.items()], str(self.binary)]
        self.process = self.node.start([*command, "--hidden"], self.log)
        pattern = re.compile(r"control channel on http://127\.0\.0\.1:(\d+)/")
        for _ in range(400):
            found = pattern.search(self.log.read_text()) if self.log.exists() else None
            if found:
                self.port = int(found.group(1))
                self.ports_before = self.udp_ports()
                return
            time.sleep(0.05)
        raise RuntimeError(f"{self.name}: no control port in {self.log}")

    def call(self, path: str, body: str | None = None, port: int | None = None) -> str:
        url = f"http://127.0.0.1:{port or self.port}{path}"
        command = ["curl", "-s", "--max-time", "5", url]
        if body is not None:
            command += ["-X", "POST", "--data-binary", body]
        return self.node.run(*command)

    def udp_ports(self) -> set[str]:
        """What is bound to a UDP port in this machine's namespace."""
        lines = self.node.run("ss", "-ulnH").splitlines()
        return {line.split()[3] for line in lines}

    def state(self) -> dict:
        return json.loads(self.call("/state"))

    def press(self, *buttons: str) -> None:
        for button in buttons:
            self.call("/input", button)
            time.sleep(0.3)

    def type_text(self, text: str) -> None:
        self.call("/text", text)
        time.sleep(0.3)

    def frame(self, out: Path, port: int | None = None) -> None:
        out.parent.mkdir(parents=True, exist_ok=True)
        self.node.run(
            "curl",
            "-s",
            "--max-time",
            "5",
            "-o",
            str(out),
            f"http://127.0.0.1:{port or self.port}/frame.png",
        )

    def wait(self, what: str, check, seconds: float = 15.0) -> dict:
        """The state once `check` holds of it."""
        until = time.monotonic() + seconds
        state = self.state()
        while time.monotonic() < until:
            state = self.state()
            if check(state):
                return state
            time.sleep(0.1)
        raise RuntimeError(f"{self.name}: never {what}; last state {summary(state)}")

    def wait_step(self, *steps: str, seconds: float = 15.0) -> dict:
        return self.wait(f"at {steps}", lambda state: state["onlineStep"] in steps, seconds)

    def focus(self, target: str, button: str = "down", most: int = 30) -> None:
        for _ in range(most):
            if self.state()["screenFocus"] == target:
                return
            self.press(button)
        raise RuntimeError(f"{self.name}: never focused {target}: {summary(self.state())}")

    def control_file(self) -> Path:
        assert self.process is not None
        return Path(os.environ["XDG_RUNTIME_DIR"]) / "opensu" / f"retro-{self.process.pid}-0.json"

    def signal_runner(self, stop: signal.Signals) -> None:
        os.kill(int(json.loads(self.control_file().read_text())["pid"]), stop)

    def runner_port(self, seconds: float = 20.0) -> int:
        """The runner's control port, from the file it hands the shell."""
        control = self.control_file()
        until = time.monotonic() + seconds
        while time.monotonic() < until:
            if control.exists():
                text = control.read_text()
                if text:
                    announced = json.loads(text)
                    if "port" in announced:
                        return int(announced["port"])
            time.sleep(0.1)
        raise RuntimeError(f"{self.name}: no runner announced itself")

    def kill(self) -> None:
        assert self.process is not None
        self.process.kill()
        self.process.wait()
        self.process = None

    def quit(self) -> None:
        if self.process is None:
            return
        if self.process.poll() is None:
            self.call("/quit", "")
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.process = None


def summary(state: dict) -> dict:
    keys = ("screen", "screenFocus", "onlineStep", "toast", "inGame", "launching", "section")
    return {key: state.get(key) for key in keys}
