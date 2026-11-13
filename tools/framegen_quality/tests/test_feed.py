from pathlib import Path

import numpy as np
from PIL import Image

from tools.framegen_quality import feed
from tools.framegen_quality.scenes import Scene


def _recording(folder: Path, pictures: list[np.ndarray]) -> Path:
    frames = folder / "frames"
    frames.mkdir(parents=True)
    for number, picture in enumerate(pictures):
        Image.fromarray(picture, "RGB").save(frames / f"{number:06}.png")
    return frames


def _scene(feed_kind: str, phase: int = 0) -> Scene:
    return Scene("s", Path("core"), Path("rom"), 0, 0, feed=feed_kind, phase=phase)


def test_changes_drops_repeats_and_rounding(tmp_path: Path) -> None:
    drawn = np.full((4, 4, 3), 100, dtype=np.uint8)
    rounded = drawn.copy()
    rounded[1, 2, 0] = 101
    moved = drawn.copy()
    moved[0, 0, 1] = 102
    frames = _recording(tmp_path, [drawn, drawn, rounded, moved, moved])
    fed = feed.fed(_scene("changes"), frames)
    assert sorted(p.name for p in fed.glob("*.png")) == ["000000.png", "000001.png"]
    assert np.array_equal(np.asarray(Image.open(fed / "000001.png")), moved)


def test_every_keeps_repeats_and_phase_drops_the_first(tmp_path: Path) -> None:
    drawn = np.zeros((2, 2, 3), dtype=np.uint8)
    frames = _recording(tmp_path, [drawn, drawn, drawn])
    assert len(list(feed.fed(_scene("every"), frames).glob("*.png"))) == 3
    assert len(list(feed.fed(_scene("every", phase=1), frames).glob("*.png"))) == 2
