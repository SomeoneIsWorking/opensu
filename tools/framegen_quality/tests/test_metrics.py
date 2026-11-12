import math
from pathlib import Path

import numpy as np

from tools.framegen_quality import metrics
from tools.framegen_quality.generate import left_out


def _picture(seed: int) -> np.ndarray:
    return np.random.default_rng(seed).integers(0, 256, (48, 64, 3)).astype(np.float64)


def test_equal_pictures_score_perfectly() -> None:
    picture = _picture(1)
    assert metrics.psnr(picture, picture) == math.inf
    assert metrics.ssim(picture, picture) == 1.0
    assert metrics.flip(picture, picture)[0] == 0.0


def test_a_known_error_has_its_psnr() -> None:
    truth = np.full((8, 8, 3), 100.0)
    assert abs(metrics.psnr(truth, truth + 10) - 10 * math.log10(255 * 255 / 100)) < 1e-9


def test_a_further_picture_scores_worse() -> None:
    truth = _picture(2)
    near = truth + np.random.default_rng(3).normal(0, 4, truth.shape)
    far = _picture(4)
    assert metrics.ssim(truth, near) > metrics.ssim(truth, far)
    assert metrics.flip(truth, near)[0] < metrics.flip(truth, far)[0]


def test_left_out_frames_sit_between_their_real_ones(tmp_path: Path) -> None:
    for number in range(7):
        (tmp_path / f"{number:06}.png").touch()
    places = [(b.stem, m.stem, a.stem, t) for b, m, a, t in left_out(tmp_path, 3)]
    assert places == [
        ("000000", "000001", "000003", 1 / 3),
        ("000000", "000002", "000003", 2 / 3),
        ("000003", "000004", "000006", 1 / 3),
        ("000003", "000005", "000006", 2 / 3),
    ]


def _dots(places: list[tuple[int, int]]) -> np.ndarray:
    picture = np.full((64, 64, 3), 80.0)
    for y, x in places:
        picture[y : y + 3, x : x + 3] = 220.0
    return picture


def test_a_missing_shape_loses_detail_and_flickers() -> None:
    before, truth, after = _dots([(20, 20)]), _dots([(20, 22)]), _dots([(20, 24)])
    erased = _dots([])
    assert metrics.detail_lost(truth, truth) == 0.0
    assert metrics.detail_lost(truth, erased) == 1.0
    assert metrics.flicker(before, after, truth) < 0.05
    assert metrics.flicker(before, after, before) < 0.05
    assert metrics.flicker(before, after, erased) > 0.9
