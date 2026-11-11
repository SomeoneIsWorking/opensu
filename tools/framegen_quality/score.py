"""Each generated frame scored against the real one it stands for, per scene and generator.

A frame counts as moving when the real frames on either side of it differ; still frames are
trivially right for every generator, so the tables are over moving frames."""

import json
import math
from concurrent.futures import ProcessPoolExecutor
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np

from tools.framegen_quality import metrics
from tools.framegen_quality.generate import left_out
from tools.framegen_quality.images import load_rgb

# A frame equal to the real one has infinite PSNR; it counts as this many dB.
PSNR_CAP = 100.0


@dataclass(frozen=True)
class FrameScore:
    frame: str
    t: float
    moving: bool
    psnr: float
    ssim: float
    flip: float


def _one(place: tuple[Path, Path, Path, float], made: Path) -> FrameScore:
    before, between, after, t = place
    truth = load_rgb(between)
    picture = load_rgb(made / between.name)
    if picture.shape != truth.shape:
        raise SystemExit(f"{made / between.name} is not the size of {between}")
    flip_mean, _ = metrics.flip(truth, picture)
    return FrameScore(
        frame=between.name,
        t=t,
        moving=not np.array_equal(load_rgb(before), load_rgb(after)),
        psnr=min(metrics.psnr(truth, picture), PSNR_CAP),
        ssim=metrics.ssim(truth, picture),
        flip=flip_mean,
    )


def score(frames: Path, made: Path, step: int) -> list[FrameScore]:
    places = left_out(frames, step)
    with ProcessPoolExecutor() as pool:
        return list(pool.map(_one, places, [made] * len(places)))


def save(scores: list[FrameScore], path: Path) -> None:
    path.write_text(json.dumps([asdict(s) for s in scores], indent=1))


def load(path: Path) -> list[FrameScore]:
    return [FrameScore(**entry) for entry in json.loads(path.read_text())]


@dataclass(frozen=True)
class Summary:
    frames: int
    moving: int
    psnr_mean: float
    psnr_p5: float
    ssim_mean: float
    flip_mean: float
    flip_p95: float


def summarize(scores: list[FrameScore]) -> Summary:
    moving = [s for s in scores if s.moving]
    if not moving:
        nan = math.nan
        return Summary(len(scores), 0, nan, nan, nan, nan, nan)
    psnr = np.array([s.psnr for s in moving])
    flip = np.array([s.flip for s in moving])
    return Summary(
        frames=len(scores),
        moving=len(moving),
        psnr_mean=float(psnr.mean()),
        psnr_p5=float(np.percentile(psnr, 5)),
        ssim_mean=float(np.mean([s.ssim for s in moving])),
        flip_mean=float(flip.mean()),
        flip_p95=float(np.percentile(flip, 95)),
    )
