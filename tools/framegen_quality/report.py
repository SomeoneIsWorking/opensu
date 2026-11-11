"""The score tables and the worst frames side by side.

`report.md` has one table row per scene and generator; `worst/<scene>-<frame>.png` shows, for the
frames the first generator named does worst on (by FLIP), the real frames before and after, the
real frame left out, and each generator's frame above its FLIP error map."""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from tools.framegen_quality import metrics
from tools.framegen_quality.generate import left_out
from tools.framegen_quality.images import load_rgb
from tools.framegen_quality.score import FrameScore, Summary, summarize


def table(results: dict[str, dict[str, list[FrameScore]]]) -> str:
    lines = [
        "| scene | generator | frames | moving | PSNR mean (dB) | PSNR worst 5% | SSIM mean "
        "| FLIP mean | FLIP worst 5% |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for scene, generators in results.items():
        for generator, scores in generators.items():
            s: Summary = summarize(scores)
            lines.append(
                f"| {scene} | {generator} | {s.frames} | {s.moving} | {s.psnr_mean:.2f} "
                f"| {s.psnr_p5:.2f} | {s.ssim_mean:.4f} | {s.flip_mean:.4f} | {s.flip_p95:.4f} |"
            )
    return "\n".join(lines) + "\n"


def _labelled(picture: np.ndarray, label: str) -> Image.Image:
    image = Image.fromarray(np.clip(np.rint(picture), 0, 255).astype(np.uint8), "RGB")
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 8 + 7 * len(label), 16), fill=(0, 0, 0))
    draw.text((4, 2), label, fill=(255, 255, 0))
    return image


def _grid(rows: list[list[Image.Image]]) -> Image.Image:
    width, height = rows[0][0].size
    columns = max(len(row) for row in rows)
    sheet = Image.new("RGB", (width * columns, height * len(rows)), (40, 40, 40))
    for y, row in enumerate(rows):
        for x, image in enumerate(row):
            sheet.paste(image, (x * width, y * height))
    return sheet


def worst(
    scene: str,
    frames: Path,
    made: dict[str, Path],
    scores: dict[str, list[FrameScore]],
    step: int,
    into: Path,
    count: int,
) -> list[Path]:
    into.mkdir(parents=True, exist_ok=True)
    first = next(iter(scores))
    by_frame = {g: {s.frame: s for s in each} for g, each in scores.items()}
    picked = sorted((s for s in scores[first] if s.moving), key=lambda s: s.flip, reverse=True)[
        :count
    ]
    places = {
        between.name: (before, between, after)
        for before, between, after, _ in left_out(frames, step)
    }
    written = []
    for pick in picked:
        before, between, after = places[pick.frame]
        truth = load_rgb(between)
        top = [
            _labelled(load_rgb(before), f"real before {before.stem}"),
            _labelled(truth, f"real {between.stem} (left out)"),
            _labelled(load_rgb(after), f"real after {after.stem}"),
        ]
        made_row, error_row = [], []
        for generator, folder in made.items():
            picture = load_rgb(folder / pick.frame)
            score = by_frame[generator][pick.frame]
            _, error = metrics.flip(truth, picture)
            error = np.squeeze(error)
            heat = np.repeat(np.clip(error * 255, 0, 255)[..., None], 3, axis=2)
            made_row.append(
                _labelled(picture, f"{generator}: {score.psnr:.1f} dB, FLIP {score.flip:.3f}")
            )
            error_row.append(_labelled(heat, f"{generator} FLIP error"))
        path = into / f"{scene}-{between.stem}.png"
        _grid([top, made_row, error_row]).save(path)
        written.append(path)
    return written
