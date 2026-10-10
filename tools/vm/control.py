"""openSU's loopback control channel, reached from inside the guest (no host port forward)."""

import shlex
from pathlib import Path

from tools.vm.guest import Guest

CONTROL_URL = "http://127.0.0.1:7311"


def state(guest: Guest) -> str:
    return guest.run(f"curl -sf {CONTROL_URL}/state").out


def press(guest: Guest, button: str) -> str:
    """Inject a pad button (`a`, `down`, `guide`, ...; add ` keyboard` for a key press)."""
    return guest.run(f"curl -sf -d {shlex.quote(button)} {CONTROL_URL}/input").out


def key(guest: Guest, chord: str) -> str:
    return guest.run(f"curl -sf -d {shlex.quote(chord)} {CONTROL_URL}/key").out


def frame(guest: Guest, local: Path) -> Path:
    """The shell's drawn frame as PNG, whatever the display shows."""
    guest.run(f"curl -sf -o /tmp/opensu-frame.png {CONTROL_URL}/frame.png")
    local.parent.mkdir(parents=True, exist_ok=True)
    guest.get("/tmp/opensu-frame.png", local)
    return local


def text(guest: Guest, value: str) -> str:
    """Type `value` as a physical keyboard would (a focused text or password field)."""
    return guest.run(f"curl -sf --data-binary {shlex.quote(value)} {CONTROL_URL}/text").out
