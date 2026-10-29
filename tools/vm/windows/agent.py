"""The QEMU guest agent: commands and files in the Windows guest over a virtio serial port.

The agent runs as a service (LocalSystem, session 0), so what it runs has no desktop; `desktop`
runs programs in the test user's session through it.
"""

import base64
import json
import random
import socket
import time
from dataclasses import dataclass
from pathlib import Path

CHUNK = 1 << 20


class AgentError(RuntimeError):
    pass


@dataclass(frozen=True)
class Finished:
    code: int
    out: str
    err: str


class Agent:
    def __init__(self, socket_path: Path):
        self._path = socket_path
        self._sock: socket.socket | None = None
        self._buffer = b""

    def __enter__(self) -> "Agent":
        self.connect()
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    def connect(self, timeout_s: float = 60) -> None:
        self._sock = socket.socket(socket.AF_UNIX)
        self._sock.settimeout(timeout_s)
        self._sock.connect(str(self._path))
        self._sync()

    def close(self) -> None:
        if self._sock is not None:
            self._sock.close()
            self._sock = None

    def _send(self, message: dict) -> None:
        assert self._sock is not None
        self._sock.sendall(json.dumps(message).encode() + b"\n")

    def _receive(self) -> dict:
        assert self._sock is not None
        while b"\n" not in self._buffer:
            data = self._sock.recv(1 << 16)
            if not data:
                raise AgentError("the guest agent closed the connection")
            self._buffer += data
        line, _, self._buffer = self._buffer.partition(b"\n")
        # guest-sync-delimited answers after a 0xFF byte that resets the parser.
        return json.loads(line.lstrip(b"\xff"))

    def _sync(self) -> None:
        """Drops whatever an earlier client left unread, as the protocol prescribes."""
        token = random.randrange(1, 1 << 31)
        self._send({"execute": "guest-sync-delimited", "arguments": {"id": token}})
        while self._receive().get("return") != token:
            pass

    def execute(self, command: str, **arguments):
        message: dict = {"execute": command}
        if arguments:
            message["arguments"] = arguments
        self._send(message)
        reply = self._receive()
        if "error" in reply:
            raise AgentError(f"{command}: {reply['error'].get('desc', reply['error'])}")
        return reply.get("return")

    def run(self, path: str, args: list[str], timeout_s: float = 600) -> Finished:
        """Runs a program as LocalSystem and waits for it."""
        pid = self.execute("guest-exec", path=path, arg=args, **{"capture-output": True})["pid"]
        deadline = time.monotonic() + timeout_s
        while True:
            status = self.execute("guest-exec-status", pid=pid)
            if status["exited"]:
                return Finished(
                    code=status.get("exitcode", -1),
                    out=_decode(status.get("out-data", "")),
                    err=_decode(status.get("err-data", "")),
                )
            if time.monotonic() > deadline:
                raise AgentError(f"{path} {args} still ran after {timeout_s}s")
            time.sleep(0.5)

    def powershell(self, script: str, timeout_s: float = 600) -> Finished:
        encoded = base64.b64encode(script.encode("utf-16-le")).decode()
        return self.run(
            "powershell.exe",
            ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
             "-EncodedCommand", encoded],
            timeout_s,
        )  # fmt: skip

    def write_file(self, path: str, content: bytes) -> None:
        handle = self.execute("guest-file-open", path=path, mode="wb")
        try:
            for start in range(0, len(content), CHUNK):
                block = base64.b64encode(content[start : start + CHUNK]).decode()
                self.execute("guest-file-write", handle=handle, **{"buf-b64": block})
        finally:
            self.execute("guest-file-close", handle=handle)

    def read_file(self, path: str) -> bytes:
        handle = self.execute("guest-file-open", path=path, mode="rb")
        parts = []
        try:
            while True:
                block = self.execute("guest-file-read", handle=handle, count=CHUNK)
                parts.append(base64.b64decode(block["buf-b64"]))
                if block["eof"]:
                    break
        finally:
            self.execute("guest-file-close", handle=handle)
        return b"".join(parts)

    def exists(self, path: str) -> bool:
        try:
            self.execute("guest-file-close", handle=self.execute("guest-file-open", path=path))
        except AgentError:
            return False
        return True


def _decode(data: str) -> str:
    return base64.b64decode(data).decode("utf-8", "replace") if data else ""


def wait_ready(socket_path: Path, timeout_s: float = 900) -> None:
    """Returns once the guest agent answers, which is after Windows has started its services."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with Agent(socket_path) as agent:
                agent.execute("guest-ping")
                return
        except (OSError, AgentError, json.JSONDecodeError):
            time.sleep(5)
    raise AgentError(f"the guest agent never answered on {socket_path}")
