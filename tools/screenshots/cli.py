"""Command line: `uv run --frozen python -m tools.screenshots [--opensu PATH] [--parity DIR]`.

Builds the demo library, then runs openSU --hidden offline in a throwaway home. By default writes
one WebP per README scene to docs/screenshots/. With --parity DIR, writes a raw PNG of every scene
to DIR instead, with a fixed system volume, as references for `tools.screenshots.compare`.
"""

import argparse
import shutil
import sys
import time
from pathlib import Path

from tools.screenshots import demo, encode, mixer, offline
from tools.screenshots.scenes import README_SCENES, SCENES, Scene
from tools.screenshots.shell import HiddenShell, Profile

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "scratch" / "screenshots"
OUTPUT = REPO / "docs" / "screenshots"


def _parse(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="python -m tools.screenshots", description=__doc__)
    parser.add_argument(
        "--opensu",
        type=Path,
        default=REPO / "build" / "check" / "cmake" / "src" / "opensu",
        help="the openSU executable to capture",
    )
    parser.add_argument("--out", type=Path, default=OUTPUT, help="where README WebPs go")
    parser.add_argument(
        "--parity", type=Path, metavar="DIR", help="write every scene as PNG into DIR instead"
    )
    return parser.parse_args(argv)


def _capture(args: argparse.Namespace) -> int:
    parity: Path | None = args.parity
    magick = None if parity else encode.require_magick()
    if not args.opensu.is_file():
        print(f"no openSU executable at {args.opensu}; build it or pass --opensu", file=sys.stderr)
        return 1
    run = WORK / "run"
    if run.exists():
        shutil.rmtree(run)
    steam, roms = demo.build(run / "library", WORK / "downloads")
    tools = mixer.install(run / "tools") if parity else None
    profile = Profile(home=run / "home", steam_root=steam, rom_root=roms, tools=tools)
    frames = parity if parity else run / "frames"
    frames.mkdir(parents=True, exist_ok=True)
    scenes: tuple[Scene, ...] = SCENES if parity else README_SCENES
    with HiddenShell(args.opensu, profile, run / "opensu.log") as shell:
        shell.wait_for("installed", len(demo.STEAM_GAMES) + len(demo.ROM_GAMES))
        for scene in scenes:
            scene.reach(shell)
            time.sleep(scene.settle)
            shell.wait_for("artworkPending", 0)
            png = frames / f"{scene.name}.png"
            png.write_bytes(shell.frame())
            if magick is None:
                print(f"{scene.name}: {png}")
            else:
                webp = args.out / f"{scene.name}.webp"
                encode.to_webp(magick, png, webp)
                print(f"{scene.name}: {webp}")
            scene.leave(shell)
    return 0


def _absolute(args: argparse.Namespace) -> list[str]:
    """The arguments again with every path absolute, as the offline run starts in the repo."""
    argv = ["--opensu", str(args.opensu.resolve()), "--out", str(args.out.resolve())]
    if args.parity:
        argv += ["--parity", str(args.parity.resolve())]
    return argv


def main(argv: list[str] | None = None) -> int:
    args = _parse(sys.argv[1:] if argv is None else argv)
    if not offline.is_offline():
        # Art is fetched first, while the network is still there.
        demo.fetch_all(WORK / "downloads")
        return offline.reexec_offline(_absolute(args))
    return _capture(args)
