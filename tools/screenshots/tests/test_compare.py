import re
import shutil
import subprocess
from pathlib import Path

import pytest

from tools.screenshots import compare
from tools.screenshots.compare import Measure, Thresholds
from tools.screenshots.scenes import README_SCENES, SCENES, Picks, real_scenes

LIMITS = Thresholds(mean=1.0, max=255, fraction=0.01, pixel=16)
MAGICK = shutil.which("magick")
needs_magick = pytest.mark.skipif(MAGICK is None, reason="ImageMagick is not installed")


def test_identical_frames_measure_zero():
    assert compare.measure(bytes(12), 16) == Measure(mean=0.0, max=0, fraction=0.0)


def test_one_channel_over_counts_its_pixel():
    found = compare.measure(bytes([0, 0, 0, 0, 200, 0, 0, 0, 0, 0, 0, 0]), 16)
    assert found == Measure(mean=200 / 12, max=200, fraction=0.25)


def test_a_pixel_counts_once_however_many_channels_differ():
    found = compare.measure(bytes([90, 90, 90, 0, 0, 0]), 16)
    assert found.fraction == 0.5


def test_the_pixel_threshold_is_exclusive():
    assert compare.measure(bytes([16, 16, 16, 17, 0, 0]), 16).fraction == 0.5


def test_measure_refuses_bytes_that_are_not_rgb():
    with pytest.raises(ValueError):
        compare.measure(bytes(4), 16)


def test_judge_names_each_exceeded_limit():
    assert compare.judge(Measure(0.5, 255, 0.005), LIMITS) == ()
    problems = compare.judge(Measure(2.0, 255, 0.02), Thresholds(1.0, 100, 0.01, 16))
    assert [problem.split()[0] for problem in problems] == ["mean", "max", "fraction"]


def _png(path: Path, *draw: str, size: str = "40x20") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([MAGICK, "-size", size, "xc:white", *draw, str(path)], check=True)


@needs_magick
def test_compare_sets_passes_equal_frames_and_fails_changed_ones(tmp_path: Path):
    ref, new, out = tmp_path / "ref", tmp_path / "new", tmp_path / "diff"
    _png(ref / "same.png")
    _png(new / "same.png")
    _png(ref / "changed.png")
    _png(new / "changed.png", "-fill", "black", "-draw", "rectangle 0,0 9,9")

    results = {r.name: r for r in compare.compare_sets(MAGICK, ref, new, out, LIMITS)}

    assert results["same"].passed
    assert results["same"].measure == Measure(0.0, 0, 0.0)
    changed = results["changed"]
    assert not changed.passed
    assert changed.measure is not None
    assert changed.measure.max == 255
    assert changed.measure.fraction == pytest.approx(100 / 800)
    assert (out / "changed.png").is_file()


@needs_magick
def test_compare_sets_fails_missing_extra_and_resized_scenes(tmp_path: Path):
    ref, new, out = tmp_path / "ref", tmp_path / "new", tmp_path / "diff"
    _png(ref / "gone.png")
    _png(ref / "resized.png")
    _png(new / "resized.png", size="40x21")
    _png(new / "unknown.png")

    results = {r.name: r for r in compare.compare_sets(MAGICK, ref, new, out, LIMITS)}

    assert results["gone"].problems == ("missing from the new set",)
    assert "size" in results["resized"].problems[0]
    assert results["unknown"].problems == ("has no reference",)


@needs_magick
def test_main_exits_one_when_a_scene_fails(tmp_path: Path):
    ref, new = tmp_path / "ref", tmp_path / "new"
    _png(ref / "a.png")
    _png(new / "a.png")
    assert compare.main([str(ref), str(new), "--out", str(tmp_path / "d")]) == 0
    _png(new / "a.png", "-fill", "black", "-draw", "rectangle 0,0 39,19")
    assert compare.main([str(ref), str(new), "--out", str(tmp_path / "d")]) == 1


def test_real_scenes_are_the_readme_set():
    readme = (Path(__file__).resolve().parents[3] / "README.md").read_text()
    shown = re.findall(r"docs/screenshots/([a-z-]+)\.webp", readme)
    assert [scene.name for scene in real_scenes(Picks())] == shown
    assert [scene.name for scene in README_SCENES] == [
        "home", "library", "details", "guide", "settings", "search",
    ]  # fmt: skip
    assert len({scene.name for scene in SCENES}) == len(SCENES)
