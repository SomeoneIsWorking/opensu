"""Commands and file transfer in the guest over SSH (user-mode forward)."""

import shlex
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path

from tools.vm.config import TEST_PASSWORD, TEST_USER, VmConfig


@dataclass(frozen=True)
class Result:
    code: int
    out: str


class Guest:
    def __init__(self, cfg: VmConfig):
        self._cfg = cfg

    def _ssh_options(self) -> list[str]:
        return [
            "-i", str(self._cfg.ssh_key), "-p", str(self._cfg.ssh_port),
            "-o", "StrictHostKeyChecking=no", "-o", f"UserKnownHostsFile={self._cfg.known_hosts}",
            "-o", "BatchMode=yes", "-o", "LogLevel=ERROR", "-o", "ConnectTimeout=5",
        ]  # fmt: skip

    def run(
        self, command: str, sudo: bool = False, timeout: int = 600, check: bool = True
    ) -> Result:
        """Run a shell command as the test user, or as root through its password."""
        stdin = None
        if sudo:
            command = f"sudo -S -p '' sh -c {shlex.quote(command)}"
            stdin = TEST_PASSWORD + "\n"
        argv = ["ssh", *self._ssh_options(), f"{TEST_USER}@127.0.0.1", command]
        done = subprocess.run(
            argv, input=stdin, capture_output=True, text=True, timeout=timeout, check=False
        )
        out = done.stdout + done.stderr
        if check and done.returncode != 0:
            raise RuntimeError(f"guest command failed ({done.returncode}): {command}\n{out}")
        return Result(done.returncode, out)

    def _scp(self, source: str, target: str) -> None:
        options = self._ssh_options()
        options[options.index("-p")] = "-P"
        argv = ["scp", "-q", "-r", *options, source, target]
        subprocess.run(argv, check=True, capture_output=True, text=True)

    def put(self, local: Path, remote: str) -> None:
        self._scp(str(local), f"{TEST_USER}@127.0.0.1:{remote}")

    def get(self, remote: str, local: Path) -> None:
        self._scp(f"{TEST_USER}@127.0.0.1:{remote}", str(local))

    def install_file(self, content: bytes, remote: str, mode: str = "644") -> None:
        """Write content to a root-owned path."""
        with tempfile.NamedTemporaryFile() as staged:
            staged.write(content)
            staged.flush()
            self.put(Path(staged.name), "/tmp/vm-staged")
        self.run(
            f"install -Dm{mode} /tmp/vm-staged {shlex.quote(remote)} && rm /tmp/vm-staged",
            sudo=True,
        )

    def wait_ready(self, timeout: int = 300) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                if self.run("true", timeout=15, check=False).code == 0:
                    return
            except subprocess.TimeoutExpired:
                pass
            time.sleep(3)
        raise RuntimeError("the guest never accepted SSH")
