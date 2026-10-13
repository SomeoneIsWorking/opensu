"""What one nested Gamescope run's exit status and log say about the host connection."""

import re
from dataclasses import dataclass

PROTOCOL_ERROR = re.compile(r"protocol error (\d+) on (\w+)@(\d+)")
WAYLAND_ERROR = re.compile(r"wl_display#1\.error\((\w+)#(\d+), (\d+), \"([^\"]*)\"\)")


@dataclass(frozen=True)
class Verdict:
    exit_code: int
    protocol_error: str | None
    host_lost: bool = False

    @property
    def passed(self) -> bool:
        """A run with its host intact exits cleanly; one whose host went away exits with a
        failure status (positive: not killed by a signal, and not a clean quit)."""
        if self.host_lost:
            return self.exit_code > 0
        return self.exit_code == 0 and self.protocol_error is None

    def describe(self) -> str:
        if self.host_lost:
            state = "ended with a failure status" if self.passed else "did not end as expected"
        else:
            state = "survived" if self.passed else "died"
        detail = f" ({self.protocol_error})" if self.protocol_error else ""
        return f"exit {self.exit_code}, {state}{detail}"


def judge(exit_code: int, log: str, host_lost: bool = False) -> Verdict:
    """The host's error message when WAYLAND_DEBUG caught it, else Gamescope's own line."""
    detailed = WAYLAND_ERROR.search(log)
    if detailed:
        interface, object_id, code, message = detailed.groups()
        return Verdict(exit_code, f"{interface}#{object_id} error {code}: {message}", host_lost)
    short = PROTOCOL_ERROR.search(log)
    if short:
        code, interface, object_id = short.groups()
        return Verdict(exit_code, f"{interface}#{object_id} error {code}", host_lost)
    return Verdict(exit_code, None, host_lost)
