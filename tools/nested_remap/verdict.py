"""What one nested Gamescope run's exit status and log say about the host connection."""

import re
from dataclasses import dataclass

PROTOCOL_ERROR = re.compile(r"protocol error (\d+) on (\w+)@(\d+)")
WAYLAND_ERROR = re.compile(r"wl_display#1\.error\((\w+)#(\d+), (\d+), \"([^\"]*)\"\)")


@dataclass(frozen=True)
class Verdict:
    exit_code: int
    protocol_error: str | None

    @property
    def survived(self) -> bool:
        return self.exit_code == 0 and self.protocol_error is None


def judge(exit_code: int, log: str) -> Verdict:
    """The host's error message when WAYLAND_DEBUG caught it, else Gamescope's own line."""
    detailed = WAYLAND_ERROR.search(log)
    if detailed:
        interface, object_id, code, message = detailed.groups()
        return Verdict(exit_code, f"{interface}#{object_id} error {code}: {message}")
    short = PROTOCOL_ERROR.search(log)
    if short:
        code, interface, object_id = short.groups()
        return Verdict(exit_code, f"{interface}#{object_id} error {code}")
    return Verdict(exit_code, None)
