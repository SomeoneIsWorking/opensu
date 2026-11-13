"""The frames a generator is fed from a scene's recording, numbered from 0 in `fed/` beside it.

`every` is every recorded frame: the built-in runner hands its generator each frame the core runs,
a repeat included. `changes` keeps a frame only when it changed from the one before by more than
one code value somewhere: Gamescope's generator sees only the game's commits, and a frame the game
did not draw comes back equal or off by rounding."""

import os
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

from tools.framegen_quality.scenes import Scene

# The least a frame the game drew again can differ by: one code value of an 8-bit channel.
ROUNDING = 1


def _pixels(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.int16)


def changes(recorded: list[Path]) -> list[Path]:
    """The first frame and each one that differs from the frame before by more than rounding."""
    kept = recorded[:1]
    before = _pixels(recorded[0]) if recorded else None
    for path in recorded[1:]:
        picture = _pixels(path)
        if int(np.abs(picture - before).max()) > ROUNDING:
            kept.append(path)
        before = picture
    return kept


def fed(scene: Scene, recorded: Path) -> Path:
    """`recorded`'s frames as `scene` feeds them, linked into `fed/` beside it."""
    every = sorted(recorded.glob("*.png"))
    picked = (every if scene.feed == "every" else changes(every))[scene.phase :]
    into = recorded.parent / "fed"
    if into.exists():
        shutil.rmtree(into)
    into.mkdir()
    for number, source in enumerate(picked):
        os.link(source, into / f"{number:06}.png")
    print(f"{scene.name}: fed {len(picked)} of {len(every)} frames ({scene.feed})", flush=True)
    return into
