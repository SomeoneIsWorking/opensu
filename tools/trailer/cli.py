"""Command line: `uv run --frozen --group sounds python -m tools.trailer [--only NAME,...]`.

Records the trailer's shots from openSU --hidden, on the player's own home and then on the
screenshot tool's homebrew-game home (scratch/screenshots/retro/, where openSU downloads the
core and shader pack the first time), and cuts them with a title card, captions, an end card and
music made for it (music.py) into scratch/trailer/.

`--only` records just those shots again and reuses the kept takes of the rest; `--cut-only`
records nothing. Like the screenshot tool's real profile, it refuses while another openSU runs and
puts the player's settings files back afterwards.
"""

import argparse
import os
import sys
import time
from pathlib import Path

from tools.checkout import REPO
from tools.screenshots import real, retro
from tools.screenshots.cli import WORK as SCREENSHOTS
from tools.screenshots.cli import _library_loaded, _retro_loaded
from tools.screenshots.shell import HiddenShell, Home, RealHome
from tools.trailer import edit, music
from tools.trailer.recorder import Recorder, encode
from tools.trailer.shots import Shot, game_shots, home_shots

WORK = REPO / "scratch" / "trailer"
TITLE = [
    edit.Line("openSU", 190),
    edit.Line("One gamepad-first shell for your whole game library", 52, "0xC9CED5"),
]
END = [
    edit.Line("openSU", 150),
    edit.Line("Linux  ·  Windows  ·  Android", 52),
    edit.Line("Free and open source, MIT", 44, "0xC9CED5"),
    edit.Line("github.com/SomeoneIsWorking/opensu", 46, edit.ACCENT),
]


def _parse(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="python -m tools.trailer", description=__doc__)
    parser.add_argument(
        "--opensu",
        type=Path,
        default=REPO / "build" / "check" / "cmake" / "src" / "opensu",
        help="the openSU executable to record",
    )
    parser.add_argument("--out", type=Path, default=WORK / "opensu-trailer.mp4")
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--only", default="", help="record only these shots, comma-separated")
    parser.add_argument("--cut-only", action="store_true", help="record nothing, cut the takes")
    return parser.parse_args(argv)


def _record(
    opensu: Path,
    home: Home,
    log: Path,
    shots: tuple[Shot, ...],
    wanted: set[str],
    fmt: edit.Format,
    loaded,
) -> None:
    if not any(shot.name in wanted for shot in shots):
        return
    with HiddenShell(opensu, home, log) as shell:
        loaded(shell)
        for shot in shots:
            shot.reach(shell)
            time.sleep(0.6)
            if shot.name in wanted:
                with Recorder(shot.sources(shell), WORK / "takes" / shot.name) as recorder:
                    shot.act(shell)
                take = recorder.take
                rate = len(take.times) / take.seconds if take.seconds else 0.0
                clip = encode(
                    take, WORK / "clips" / f"{shot.name}.mp4", fmt.width, fmt.height, fmt.fps
                )
                print(f"{shot.name}: {take.seconds:.1f} s at {rate:.0f} frames/s -> {clip}")
            else:
                shot.act(shell)
            shot.leave(shell)


def main(argv: list[str] | None = None) -> int:
    args = _parse(sys.argv[1:] if argv is None else argv)
    fmt = edit.Format(args.width, args.height, args.fps)
    os.environ["OPENSU_WIDTH"] = str(fmt.width)
    os.environ["OPENSU_HEIGHT"] = str(fmt.height)
    os.environ.setdefault("OPENSU_DISPLAY_SCALE", f"{fmt.height / 720:g}")
    retro_home = retro.profile(SCREENSHOTS / "retro", SCREENSHOTS / "downloads")
    shots = home_shots() + game_shots(retro_home)
    names = {shot.name for shot in shots}
    wanted = set() if args.cut_only else (set(filter(None, args.only.split(","))) or names)
    if wanted - names:
        print(
            f"no shots named {sorted(wanted - names)}; there are {sorted(names)}", file=sys.stderr
        )
        return 1
    (WORK / "clips").mkdir(parents=True, exist_ok=True)
    if wanted:
        if not args.opensu.is_file():
            print(
                f"no openSU executable at {args.opensu}; build it or pass --opensu", file=sys.stderr
            )
            return 1
        with real.KeptSettings(WORK / "settings-backup"):
            real.choose_console_theme(real.DEFAULT_CONSOLE_THEME)
            _record(
                args.opensu,
                RealHome(),
                WORK / "opensu.log",
                home_shots(),
                wanted,
                fmt,
                _library_loaded,
            )
        _record(
            args.opensu,
            retro_home,
            WORK / "opensu-retro.log",
            game_shots(retro_home),
            wanted,
            fmt,
            _retro_loaded(retro_home),
        )
    cut = WORK / "cut"
    cut.mkdir(parents=True, exist_ok=True)
    parts = [edit.card(TITLE, 4.0, fmt, cut, cut / "00-title.mp4")]
    for index, shot in enumerate(shots, start=1):
        clip = WORK / "clips" / f"{shot.name}.mp4"
        if not clip.is_file():
            print(f"no take of {shot.name} yet; record it with --only {shot.name}", file=sys.stderr)
            return 1
        parts.append(
            edit.captioned(
                clip, shot.caption, shot.longest, fmt, cut, cut / f"{index:02d}-{shot.name}.mp4"
            )
        )
    parts.append(edit.card(END, 6.0, fmt, cut, cut / f"{len(parts):02d}-end.mp4"))
    length = edit.total_length(parts)
    track = music.render(length, WORK / "music.wav")
    out = edit.assemble(parts, track, fmt, args.out)
    print(f"trailer: {out} ({length:.1f} s)")
    return 0
