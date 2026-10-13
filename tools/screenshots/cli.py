"""Command line: `uv run --frozen python -m tools.screenshots [--opensu PATH]`.

Builds the demo library, then runs openSU --hidden offline in a throwaway home and writes one
WebP per scene to docs/screenshots/.
"""

import argparse
import shutil
import sys
import time
from pathlib import Path

from tools.screenshots import demo, encode, offline
from tools.screenshots.scenes import SCENES
from tools.screenshots.shell import HiddenShell, Profile

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "scratch" / "screenshots"
OUTPUT = REPO / "docs" / "screenshots"
# Long enough for a panel's slide and the focus ring's entrance to finish.
SETTLE_SECONDS = 1.2


def _parse(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="python -m tools.screenshots", description=__doc__)
    parser.add_argument(
        "--opensu",
        type=Path,
        default=REPO / "build" / "check" / "cmake" / "src" / "opensu",
        help="the openSU executable to capture",
    )
    parser.add_argument("--out", type=Path, default=OUTPUT)
    return parser.parse_args(argv)


def _capture(args: argparse.Namespace) -> int:
    magick = encode.require_magick()
    if not args.opensu.is_file():
        print(f"no openSU executable at {args.opensu}; build it or pass --opensu", file=sys.stderr)
        return 1
    run = WORK / "run"
    if run.exists():
        shutil.rmtree(run)
    steam, roms = demo.build(run / "library", WORK / "downloads")
    profile = Profile(home=run / "home", steam_root=steam, rom_root=roms)
    frames = run / "frames"
    frames.mkdir(parents=True)
    with HiddenShell(args.opensu, profile, run / "opensu.log") as shell:
        shell.wait_for("installed", len(demo.STEAM_GAMES) + len(demo.ROM_GAMES))
        for scene in SCENES:
            scene.reach(shell)
            time.sleep(SETTLE_SECONDS)
            png = frames / f"{scene.name}.png"
            png.write_bytes(shell.frame())
            encode.to_webp(magick, png, args.out / f"{scene.name}.webp")
            print(f"{scene.name}: {args.out / (scene.name + '.webp')}")
            scene.leave(shell)
    return 0


def main(argv: list[str] | None = None) -> int:
    arguments = sys.argv[1:] if argv is None else argv
    if not offline.is_offline():
        # Art is fetched first, while the network is still there.
        demo.fetch_all(WORK / "downloads")
        return offline.reexec_offline(arguments)
    return _capture(_parse(arguments))
