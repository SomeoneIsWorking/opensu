"""The scenes a run records, read from a JSON file the maintainer keeps outside the tree (cores and
games are the player's own):

    [{"name": "metroid-intro", "core": "/path/core.so", "rom": "/path/game.sfc",
      "skip": 600, "frames": 240, "presses": [{"at": 300, "buttons": ["start"], "for": 4}]}]

`skip` frames run before recording starts, `frames` are recorded, and each press holds its buttons
from frame `at` for `for` frames (default 4). `pixel` is how many recorded pixels one of the
core's takes across and down (default [1, 1]), as the runner scaled its frame to the window.

`feed` is what the generator is given of the recording (`feed`): `every` frame the core ran (the
built-in runner's generator, the default) or only the `changes` (Gamescope's, which sees the
game's commits). `phase` frames of the fed ones are dropped first, so a scene can be held out on
its other frames."""

import json
from dataclasses import dataclass, field
from pathlib import Path

FEEDS = ("every", "changes")


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
    pixel: tuple[float, float] = (1.0, 1.0)
    feed: str = "every"
    phase: int = 0


def _pixel(given: list[float]) -> tuple[float, float]:
    across, down = (float(v) for v in given)
    return across, down


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
                pixel=_pixel(entry.get("pixel", [1, 1])),
                feed=entry.get("feed", "every"),
                phase=int(entry.get("phase", 0)),
            )
        )
        if scenes[-1].feed not in FEEDS:
            raise SystemExit(f"{path}: {scenes[-1].name}'s feed is not one of {', '.join(FEEDS)}")
    names = [scene.name for scene in scenes]
    if len(set(names)) != len(names):
        raise SystemExit(f"{path}: scene names repeat")
    return scenes
