"""The scenes a run records, read from a JSON file the maintainer keeps outside the tree (cores and
games are the player's own):

    [{"name": "metroid-intro", "core": "/path/core.so", "rom": "/path/game.sfc",
      "skip": 600, "frames": 240, "presses": [{"at": 300, "buttons": ["start"], "for": 4}]}]

`skip` frames run before recording starts, `frames` are recorded, and each press holds its buttons
from frame `at` for `for` frames (default 4)."""

import json
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class Press:
    at: int
    buttons: tuple[str, ...]
    frames: int = 4


@dataclass(frozen=True)
class Scene:
    name: str
    core: Path
    rom: Path
    skip: int
    frames: int
    presses: tuple[Press, ...] = field(default_factory=tuple)
    options: tuple[str, ...] = field(default_factory=tuple)


def read(path: Path) -> list[Scene]:
    scenes = []
    for entry in json.loads(path.read_text()):
        scenes.append(
            Scene(
                name=entry["name"],
                core=Path(entry["core"]),
                rom=Path(entry["rom"]),
                skip=int(entry.get("skip", 0)),
                frames=int(entry["frames"]),
                presses=tuple(
                    Press(int(p["at"]), tuple(p["buttons"]), int(p.get("for", 4)))
                    for p in entry.get("presses", [])
                ),
                options=tuple(entry.get("options", [])),
            )
        )
    names = [scene.name for scene in scenes]
    if len(set(names)) != len(names):
        raise SystemExit(f"{path}: scene names repeat")
    return scenes
