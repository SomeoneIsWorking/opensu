"""Whether two runs made the same frames: every made PNG's RGBA compared bit for bit, the
reference's taken at the centres of its `scale` by `scale` pixels when it was made `scale` times
larger (flow's `--content-scale` against flow on frames scaled up nearest)."""

from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image


@dataclass(frozen=True)
class Difference:
    frames: int
    differing: list[str]
    missing: list[str]


def _rgba(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA"))


def _centres(picture: np.ndarray, scale: int) -> np.ndarray:
    return picture[scale // 2 :: scale, scale // 2 :: scale]


def compare(made: Path, reference: Path, scale: int = 1) -> Difference:
    """`made`'s frames against `reference`'s of the same names, both ways."""
    ours = {p.name for p in made.glob("*.png")}
    theirs = {p.name for p in reference.glob("*.png")}
    differing = [
        name
        for name in sorted(ours & theirs)
        if not np.array_equal(_rgba(made / name), _centres(_rgba(reference / name), scale))
    ]
    return Difference(len(ours & theirs), differing, sorted(ours ^ theirs))
