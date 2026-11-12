"""uv run --frozen --group framegen python -m tools.framegen_quality --scenes FILE [options]

Records each scene once (`scratch/framegen-quality/scenes/<scene>/frames`, again with --record),
then for the run (`scratch/framegen-quality/runs/<run>/`) generates the left-out frames with each
generator, scores them, and writes `report.md`, `scores/<scene>-<generator>.json`, the worst
frames side by side in `worst/` and the build that made them in `build.json`
(`provenance`)."""

import argparse
from pathlib import Path

from tools.checkout import REPO, require_own_environment
from tools.framegen_quality import generate, provenance, record, report, scenes, score

SCRATCH = REPO / "scratch" / "framegen-quality"


def _binary(build: Path, name: str) -> Path | None:
    for candidate in (build / "src" / name, build / "src" / "frame_gen" / name):
        if candidate.is_file():
            return candidate
    return None


def main() -> None:
    require_own_environment()
    parser = argparse.ArgumentParser(
        prog="tools.framegen_quality", description=__doc__.splitlines()[2]
    )
    parser.add_argument(
        "--scenes",
        type=Path,
        required=True,
        help="the scenes' JSON file (tools/framegen_quality/scenes.py)",
    )
    parser.add_argument(
        "--only", action="append", default=[], help="run this scene only (repeatable)"
    )
    parser.add_argument("--run", default="latest", help="the run's folder name")
    parser.add_argument(
        "--generators",
        default="lossless,flow,blend,repeat",
        help="comma-separated; the first is the one worst frames are picked by",
    )
    parser.add_argument(
        "--step", type=int, default=2, help="every step-th frame is real (2: half the rate)"
    )
    parser.add_argument(
        "--dll",
        type=Path,
        help="the player's Lossless.dll; default: the one in their Steam libraries",
    )
    parser.add_argument("--build", type=Path, default=REPO / "build" / "agent" / "cmake")
    parser.add_argument("--record", action="store_true", help="record the scenes again")
    parser.add_argument("--worst", type=int, default=3, help="worst frames saved per scene")
    parser.add_argument(
        "--rescore", action="store_true", help="score the frames the run already made again"
    )
    args = parser.parse_args()

    generators = [g for g in args.generators.split(",") if g]
    runner = _binary(args.build, "opensu-retro")
    pairs = _binary(args.build, "opensu-framegen-pairs")
    run = SCRATCH / "runs" / args.run
    if not args.rescore and pairs is not None:
        provenance.save(provenance.check(args.build, pairs), run)
    results: dict[str, dict[str, list[score.FrameScore]]] = {}
    for scene in scenes.read(args.scenes):
        if args.only and scene.name not in args.only:
            continue
        frames = SCRATCH / "scenes" / scene.name / "frames"
        if args.record or not frames.is_dir():
            if runner is None:
                raise SystemExit(f"no opensu-retro under {args.build}")
            print(f"{scene.name}: recording {scene.frames} frames after {scene.skip}", flush=True)
            record.record(scene, runner, frames.parent)
        made: dict[str, Path] = {}
        results[scene.name] = {}
        for generator in generators:
            folder = run / "made" / scene.name / generator
            log = run / "logs" / f"{scene.name}-{generator}.log"
            log.parent.mkdir(parents=True, exist_ok=True)
            if not args.rescore:
                generate.generate(
                    generator, frames, folder, args.step, pairs, args.dll, log, scene.pixel
                )
            scores = score.score(frames, folder, args.step)
            (run / "scores").mkdir(parents=True, exist_ok=True)
            score.save(scores, run / "scores" / f"{scene.name}-{generator}.json")
            results[scene.name][generator] = scores
            made[generator] = folder
            print(f"{scene.name}: {generator} scored", flush=True)
        report.worst(
            scene.name, frames, made, results[scene.name], args.step, run / "worst", args.worst
        )
    text = report.table(results)
    (run / "report.md").write_text(text)
    print(text)
