"""The frames left out, made again by each generator from the real frames around them: every
`step`-th recorded frame is real, the ones between are written under their own names.

`lossless` (the player's LSFG 3 shaders) and `flow` (openSU's own) run in
`opensu-framegen-pairs`; `repeat` (the last real frame) and `blend` (the two mixed by time) are
references to read scores by."""

import subprocess
from pathlib import Path

import numpy as np

from tools.framegen_quality.images import load_rgb, save_rgb

OPENSU = ("lossless", "flow")
REFERENCES = ("repeat", "blend")


def real_frames(frames: Path, step: int) -> list[Path]:
    every = sorted(frames.glob("*.png"))
    return every[::step]


def left_out(frames: Path, step: int) -> list[tuple[Path, Path, Path, float]]:
    """Each frame left out, with the real frames before and after it and its time between them."""
    every = sorted(frames.glob("*.png"))
    out = []
    for real in range(step, len(every), step):
        for between in range(1, step):
            out.append(
                (every[real - step], every[real - step + between], every[real], between / step)
            )
    return out


def generate(
    generator: str,
    frames: Path,
    into: Path,
    step: int,
    pairs: Path | None,
    dll: Path | None,
    log: Path,
) -> None:
    into.mkdir(parents=True, exist_ok=True)
    if generator in OPENSU:
        if pairs is None:
            raise SystemExit("opensu-framegen-pairs is not built")
        command = [
            str(pairs),
            "--generator",
            generator,
            "--step",
            str(step),
            "--in",
            str(frames),
            "--out",
            str(into),
        ]
        if generator == "lossless" and dll is not None:
            command += ["--dll", str(dll)]
        with log.open("wb") as out:
            code = subprocess.run(
                command, stdout=out, stderr=subprocess.STDOUT, check=False
            ).returncode
        if code != 0:
            raise SystemExit(f"{generator}: opensu-framegen-pairs exited {code}, see {log}")
        return
    for before, between, after, t in left_out(frames, step):
        if generator == "repeat":
            picture = load_rgb(before)
        elif generator == "blend":
            picture = np.rint(load_rgb(before) * (1 - t) + load_rgb(after) * t)
        else:
            raise SystemExit(f"no generator {generator}")
        save_rgb(picture, into / between.name)
