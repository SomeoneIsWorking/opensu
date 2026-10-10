"""QMP client: screenshots and keyboard and pointer input for the guest."""

import json
import socket
import time
from pathlib import Path

ABS_MAX = 0x7FFF

_SHIFTED = {
    "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7", "*": "8",
    "(": "9", ")": "0", "_": "minus", "+": "equal", "{": "bracket_left",
    "}": "bracket_right", "|": "backslash", ":": "semicolon", '"': "apostrophe",
    "<": "comma", ">": "dot", "?": "slash", "~": "grave_accent",
}  # fmt: skip
_PLAIN = {
    " ": "spc", "\n": "ret", "\t": "tab", "-": "minus", "=": "equal", "[": "bracket_left",
    "]": "bracket_right", "\\": "backslash", ";": "semicolon", "'": "apostrophe",
    ",": "comma", ".": "dot", "/": "slash", "`": "grave_accent",
}  # fmt: skip


def keys_for_char(char: str) -> list[str]:
    """The QMP qcodes that type one character on a US keyboard."""
    if char.isascii() and char.isalpha():
        return ["shift", char.lower()] if char.isupper() else [char]
    if char.isascii() and char.isdigit():
        return [char]
    if char in _PLAIN:
        return [_PLAIN[char]]
    if char in _SHIFTED:
        return ["shift", _SHIFTED[char]]
    raise ValueError(f"no key for {char!r}")


class QmpError(RuntimeError):
    pass


class Qmp:
    def __init__(self, socket_path: Path, width: int, height: int):
        self._path = socket_path
        self._width = width
        self._height = height
        self._sock: socket.socket | None = None
        self._reader = None

    def __enter__(self) -> "Qmp":
        self.connect()
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    def connect(self) -> None:
        self._sock = socket.socket(socket.AF_UNIX)
        self._sock.settimeout(30)
        self._sock.connect(str(self._path))
        self._reader = self._sock.makefile("r")
        self._reader.readline()
        self.execute("qmp_capabilities")

    def close(self) -> None:
        if self._sock is not None:
            self._sock.close()
            self._sock = None

    def execute(self, command: str, **arguments):
        assert self._sock is not None and self._reader is not None
        message = {"execute": command}
        if arguments:
            message["arguments"] = arguments
        self._sock.sendall((json.dumps(message) + "\n").encode())
        while True:
            reply = json.loads(self._reader.readline())
            if "return" in reply:
                return reply["return"]
            if "error" in reply:
                raise QmpError(f"{command}: {reply['error']['desc']}")

    def screendump(self, path: Path) -> Path:
        path.parent.mkdir(parents=True, exist_ok=True)
        self.execute("screendump", filename=str(path), format="png")
        return path

    def send_key(self, keys: list[str], hold_ms: int = 80) -> None:
        self.execute(
            "send-key",
            keys=[{"type": "qcode", "data": key} for key in keys],
            **{"hold-time": hold_ms},
        )

    def type_text(self, text: str, delay_s: float = 0.05) -> None:
        for char in text:
            self.send_key(keys_for_char(char))
            time.sleep(delay_s)

    def move(self, x: int, y: int) -> None:
        """Absolute pointer move in guest pixels."""
        ax = round(x * ABS_MAX / max(self._width - 1, 1))
        ay = round(y * ABS_MAX / max(self._height - 1, 1))
        self.execute(
            "input-send-event",
            events=[
                {"type": "abs", "data": {"axis": "x", "value": ax}},
                {"type": "abs", "data": {"axis": "y", "value": ay}},
            ],
        )

    def button(self, name: str, down: bool) -> None:
        self.execute(
            "input-send-event",
            events=[{"type": "btn", "data": {"button": name, "down": down}}],
        )

    def click(self, x: int, y: int, button: str = "left") -> None:
        self.move(x, y)
        time.sleep(0.15)
        self.button(button, True)
        time.sleep(0.08)
        self.button(button, False)

    def powerdown(self) -> None:
        self.execute("system_powerdown")

    def quit(self) -> None:
        self.execute("quit")
