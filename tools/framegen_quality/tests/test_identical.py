from pathlib import Path

import numpy as np
from PIL import Image

from tools.framegen_quality import identical


def _save(folder: Path, name: str, picture: np.ndarray) -> None:
    folder.mkdir(parents=True, exist_ok=True)
    Image.fromarray(picture, "RGBA").save(folder / name)


def test_one_changed_bit_is_a_difference(tmp_path: Path) -> None:
    picture = np.random.default_rng(1).integers(0, 256, (16, 16, 4)).astype(np.uint8)
    changed = picture.copy()
    changed[3, 5, 1] ^= 1
    _save(tmp_path / "a", "000001.png", picture)
    _save(tmp_path / "a", "000003.png", picture)
    _save(tmp_path / "b", "000001.png", picture)
    _save(tmp_path / "b", "000003.png", changed)
    same = identical.compare(tmp_path / "a", tmp_path / "b")
    assert same.frames == 2
    assert same.differing == ["000003.png"]
    assert same.missing == []


def test_a_frame_only_one_run_made_is_missing(tmp_path: Path) -> None:
    picture = np.zeros((4, 4, 4), dtype=np.uint8)
    _save(tmp_path / "a", "000001.png", picture)
    _save(tmp_path / "b", "000001.png", picture)
    _save(tmp_path / "b", "000003.png", picture)
    assert identical.compare(tmp_path / "a", tmp_path / "b").missing == ["000003.png"]


def test_a_larger_reference_is_read_at_its_pixels_centres(tmp_path: Path) -> None:
    picture = np.random.default_rng(2).integers(0, 256, (6, 5, 4)).astype(np.uint8)
    larger = np.repeat(np.repeat(picture, 4, axis=0), 4, axis=1)
    larger[0, 0, 0] ^= 1
    _save(tmp_path / "a", "000001.png", picture)
    _save(tmp_path / "b", "000001.png", larger)
    assert identical.compare(tmp_path / "a", tmp_path / "b", scale=4).differing == []
    larger[2, 2, 0] ^= 1
    _save(tmp_path / "b", "000001.png", larger)
    assert identical.compare(tmp_path / "a", tmp_path / "b", scale=4).differing == ["000001.png"]
