"""Command line: `uv run --frozen python -m tools.screenshots.compare REF NEW [--out DIFFDIR]`.

Compares two `tools.screenshots --parity` sets scene by scene: the mean and maximum per-channel
difference, and the fraction of pixels where any channel differs by more than --pixel. Writes a
difference image per scene (amplified, so small differences show) and exits 1 when any scene
exceeds a threshold, is missing from NEW, has no reference or differs in size.
"""

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from tools.screenshots import encode

REPO = Path(__file__).resolve().parents[2]
# Difference images are this many times brighter than the difference itself.
AMPLIFY = 8


@dataclass(frozen=True)
class Thresholds:
    """Each a limit a scene must not exceed; `pixel` is what counts a pixel as different."""

    mean: float
    max: int
    fraction: float
    pixel: int


# From three captures of one build: scenes differed by up to mean 0.52 and 0.84% of pixels over 16,
# from the focus ring's turn and the clock (whose width also moves the launcher badges), which no
# seam freezes. Both reach 255 in a channel, so the maximum is reported but not gated.
DEFAULT_THRESHOLDS = Thresholds(mean=1.0, max=255, fraction=0.015, pixel=16)


@dataclass(frozen=True)
class Measure:
    mean: float
    max: int
    fraction: float


@dataclass(frozen=True)
class SceneResult:
    name: str
    measure: Measure | None
    problems: tuple[str, ...]

    @property
    def passed(self) -> bool:
        return not self.problems


def measure(rgb: bytes, pixel: int) -> Measure:
    """The statistics of an RGB difference image: 8-bit channels, three per pixel."""
    if not rgb or len(rgb) % 3:
        raise ValueError(f"not RGB pixels: {len(rgb)} bytes")
    over = bytes(1 if value > pixel else 0 for value in range(256))
    marked = rgb.translate(over)
    # Each mark is one byte of 0 or 1, so OR-ing the three channel planes as integers leaves one
    # set bit per pixel with any channel over the threshold.
    planes = (int.from_bytes(marked[channel::3], "big") for channel in range(3))
    differing = 0
    for plane in planes:
        differing |= plane
    pixels = len(rgb) // 3
    return Measure(mean=sum(rgb) / len(rgb), max=max(rgb), fraction=differing.bit_count() / pixels)


def judge(found: Measure, limits: Thresholds) -> tuple[str, ...]:
    problems = []
    if found.mean > limits.mean:
        problems.append(f"mean {found.mean:.3f} > {limits.mean}")
    if found.max > limits.max:
        problems.append(f"max {found.max} > {limits.max}")
    if found.fraction > limits.fraction:
        problems.append(f"fraction {found.fraction:.5f} > {limits.fraction}")
    return tuple(problems)


def _size(magick: str, png: Path) -> tuple[int, int]:
    out = subprocess.run(
        [magick, "identify", "-format", "%w %h", str(png)],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.split()
    return int(out[0]), int(out[1])


def difference(magick: str, ref: Path, new: Path, image: Path) -> bytes:
    """The absolute per-channel difference of two same-sized PNGs as RGB bytes; writes `image`."""
    image.parent.mkdir(parents=True, exist_ok=True)
    command = [
        magick, str(ref), str(new),
        "-alpha", "off",
        "-compose", "difference", "-composite",
        "-depth", "8",
        "(", "+clone", "-evaluate", "multiply", str(AMPLIFY), "-write", str(image), "+delete", ")",
        "rgb:-",
    ]  # fmt: skip
    return subprocess.run(command, check=True, capture_output=True).stdout


def compare_scene(
    magick: str, ref: Path, new: Path, image: Path, limits: Thresholds
) -> SceneResult:
    name = ref.stem
    if not new.is_file():
        return SceneResult(name, None, ("missing from the new set",))
    ref_size, new_size = _size(magick, ref), _size(magick, new)
    if ref_size != new_size:
        return SceneResult(name, None, (f"size {new_size} differs from {ref_size}",))
    rgb = difference(magick, ref, new, image)
    if len(rgb) != ref_size[0] * ref_size[1] * 3:
        raise RuntimeError(f"{magick} returned {len(rgb)} bytes for {ref_size}")
    found = measure(rgb, limits.pixel)
    return SceneResult(name, found, judge(found, limits))


def compare_sets(
    magick: str, ref: Path, new: Path, out: Path, limits: Thresholds
) -> list[SceneResult]:
    references = sorted(ref.glob("*.png"))
    if not references:
        raise SystemExit(f"no reference PNGs in {ref}")
    results = [
        compare_scene(magick, png, new / png.name, out / png.name, limits) for png in references
    ]
    known = {png.name for png in references}
    for extra in sorted(new.glob("*.png")):
        if extra.name not in known:
            results.append(SceneResult(extra.stem, None, ("has no reference",)))
    return results


def report(result: SceneResult) -> str:
    verdict = "ok  " if result.passed else "FAIL"
    found = result.measure
    numbers = (
        f"mean {found.mean:7.3f}  max {found.max:3d}  over {found.fraction:8.5f}"
        if found
        else " " * 42
    )
    problems = "; ".join(result.problems)
    return f"{verdict} {result.name:20s} {numbers}  {problems}".rstrip()


def _parse(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="python -m tools.screenshots.compare",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("ref", type=Path, help="the reference set")
    parser.add_argument("new", type=Path, help="the set to check")
    parser.add_argument(
        "--out",
        type=Path,
        default=REPO / "scratch" / "parity" / "diff",
        help="where difference images go",
    )
    limits = DEFAULT_THRESHOLDS
    parser.add_argument("--mean", type=float, default=limits.mean, help="largest mean difference")
    parser.add_argument("--max", type=int, default=limits.max, help="largest channel difference")
    parser.add_argument(
        "--fraction",
        type=float,
        default=limits.fraction,
        help="largest share of pixels over --pixel",
    )
    parser.add_argument(
        "--pixel", type=int, default=limits.pixel, help="a channel difference that counts a pixel"
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse(sys.argv[1:] if argv is None else argv)
    limits = Thresholds(mean=args.mean, max=args.max, fraction=args.fraction, pixel=args.pixel)
    results = compare_sets(encode.require_magick(), args.ref, args.new, args.out, limits)
    for result in results:
        print(report(result))
    failed = [result.name for result in results if not result.passed]
    print(
        f"{len(results) - len(failed)}/{len(results)} scenes within thresholds; diffs in {args.out}"
    )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
