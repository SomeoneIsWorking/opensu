"""The real profile's guard: one openSU per user, and the player's settings as they were.

The console art theme the run shows is set in the player's settings files first, so they are put
back afterwards; the hidden openSU itself writes none of them.
"""

import json
from pathlib import Path
from types import TracebackType

PROC = Path("/proc")
EXECUTABLE = "opensu"
SETTINGS = Path.home() / ".config" / "opensu"
KEPT = ("settings.json", "game-settings.json")


def running_opensu() -> list[int]:
    """The PIDs of every openSU this user can see."""
    found = []
    for entry in PROC.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            name = (entry / "comm").read_text().strip()
        except OSError:
            continue
        if name == EXECUTABLE:
            found.append(int(entry.name))
    return found


class KeptSettings:
    """Copies the player's settings files into `backup` on entry and writes them back on exit."""

    def __init__(self, backup: Path) -> None:
        self._backup = backup
        self._saved: dict[str, bytes | None] = {}

    def __enter__(self) -> "KeptSettings":
        others = running_opensu()
        if others:
            raise SystemExit(f"openSU is already running ({others}); one openSU per user")
        self._backup.mkdir(parents=True, exist_ok=True)
        for name in KEPT:
            source = SETTINGS / name
            data = source.read_bytes() if source.exists() else None
            self._saved[name] = data
            if data is not None:
                (self._backup / name).write_bytes(data)
        return self

    def __exit__(
        self,
        _type: type[BaseException] | None,
        _value: BaseException | None,
        _traceback: TracebackType | None,
    ) -> None:
        for name, data in self._saved.items():
            target = SETTINGS / name
            current = target.read_bytes() if target.exists() else None
            if current == data:
                continue
            if data is None:
                target.unlink()
            else:
                target.write_bytes(data)
            print(f"restored {target}")


# openSU's default console art theme (`artwork::defaultConsoleTheme`), kept as no key.
DEFAULT_CONSOLE_THEME = "photos"


def choose_console_theme(theme: str) -> None:
    """Sets the console art theme openSU shows (`consoleTheme`) for the run."""
    target = SETTINGS / KEPT[0]
    values = json.loads(target.read_text()) if target.exists() else {}
    values["consoleTheme"] = "" if theme == DEFAULT_CONSOLE_THEME else theme
    target.write_text(json.dumps(values, indent=4))
