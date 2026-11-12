"""uv run --frozen --group framegen python -m tools.framegen_quality.core_frames --frames DIR

Whether flow on a core's own frames (`opensu-framegen-pairs --core-frames`, which works on them
scaled up by `frame_gen::flowContentScale`) loses nothing writing its frames back: each must equal,
at the centres of the scaled pixels, what flow makes from the same frames scaled up nearest by
that factor beforehand (`identical`). The factor is the one the pairs tool reports. Runs in
`scratch/framegen-quality/core-frames/`; exits 1 on any difference."""

import argparse
import re
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

from tools.checkout import REPO, require_own_environment
from tools.framegen_quality import generate, identical, provenance
from tools.framegen_quality.cli import SCRATCH, binary

SCALED = re.compile(r"flow works on the frames scaled up by (\d+)")


def scale_of(log: Path) -> int:
    found = SCALED.search(log.read_text())
    if found is None:
        raise SystemExit(f"{log} does not say what flow scaled the frames by")
    return int(found.group(1))


def scale_up(frames: Path, into: Path, scale: int) -> None:
    """Each frame scaled up nearest by `scale`."""
    into.mkdir(parents=True, exist_ok=True)
    for path in sorted(frames.glob("*.png")):
        with Image.open(path) as image:
            picture = np.asarray(image.convert("RGBA"))
        larger = np.repeat(np.repeat(picture, scale, axis=0), scale, axis=1)
        Image.fromarray(larger, "RGBA").save(into / path.name)


def main() -> None:
    require_own_environment()
    parser = argparse.ArgumentParser(
        prog="tools.framegen_quality.core_frames", description=__doc__.splitlines()[2]
    )
    parser.add_argument("--frames", type=Path, required=True, help="a core's own PNG frames")
    parser.add_argument("--build", type=Path, default=REPO / "build" / "agent" / "cmake")
    parser.add_argument("--step", type=int, default=2)
    args = parser.parse_args()

    pairs = binary(args.build, "opensu-framegen-pairs")
    if pairs is None:
        raise SystemExit(f"no opensu-framegen-pairs under {args.build}")
    run = SCRATCH / "core-frames"
    shutil.rmtree(run, ignore_errors=True)
    provenance.save(provenance.check(args.build, pairs), run)
    native = run / "native"
    generate.generate(
        "flow", args.frames, native, args.step, pairs, None, run / "native.log", core_frames=True
    )
    scale = scale_of(run / "native.log")
    scale_up(args.frames, run / "frames-scaled", scale)
    scaled = run / "scaled"
    generate.generate(
        "flow",
        run / "frames-scaled",
        scaled,
        args.step,
        pairs,
        None,
        run / "scaled.log",
        (float(scale), float(scale)),
    )
    same = identical.compare(native, scaled, scale)
    print(
        f"flow on core frames against flow on them scaled up by {scale}: {same.frames} frames,"
        f" {len(same.differing)} differ, {len(same.missing)} missing"
    )
    if same.differing or same.missing or not same.frames:
        raise SystemExit(f"first differing {same.differing[:3]}, missing {same.missing[:3]}")


if __name__ == "__main__":
    main()
