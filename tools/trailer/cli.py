"""Command line: `uv run --frozen --group sounds python -m tools.trailer [--only NAME,...]`.

Records the trailer's shots from openSU --hidden on the player's own profile and games, each take
recorded by openSU itself (`POST /record`) at a steady frame rate, and cuts them with a title card,
captions, an end card and music made for it (music.py) into scratch/trailer/. With `--guest`, it
also records remote play: this openSU, in a headless Gamescope, streams a game to a handheld's
openSU (remote.py).

`--only` records just those shots again and reuses the kept clips of the rest; `--cut-only`
records nothing. Like the screenshot tool's real profile, it refuses while another openSU runs and
puts the player's settings files back afterwards.
"""

import argparse
import os
import shutil
import sys
import time
from pathlib import Path

from tools.checkout import REPO
from tools.screenshots import real
from tools.screenshots.cli import _library_loaded
from tools.screenshots.shell import HiddenShell, RealHome
from tools.trailer import edit, music, remote
from tools.trailer.recorder import Recorder, encode
from tools.trailer.shots import Shot, game_shots, home_shots, theme_shots

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
# What openSU does, from its code and the guide: the README's list runs behind.
FEATURES = [
    [
        "Steam, Epic and GOG in one library",
        "Install, update, uninstall from the pad",
        "ROMs on built-in libretro cores",
        "Standalone emulators, downloaded",
        "Shader presets and save states",
        "Settings per console, core or game",
        "Button mappings for any game",
        "Windows games under Wine or Proton",
    ],
    [
        "Remote play from your Linux PC",
        "Netplay on your LAN or online",
        "Paired openSUs share ROMs and saves",
        "Gamescope, FSR and frame generation",
        "Player profiles with their own saves",
        "Free-game giveaways in Library",
        "Covers from SteamGridDB",
        "Themes, console art and Lua plugins",
    ],
]
CHANNELS = "channels"
# The order the cut shows the shots in; they are recorded in the order the shell reaches them.
CUT = (
    "home",
    "library",
    "details",
    "search",
    "arcade",
    "guide-game",
    "n64",
    "layered",
    "mapping",
    remote.SHOT.name,
    "guide",
    "settings",
    "channels",
)


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
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--only", default="", help="record only these shots, comma-separated")
    parser.add_argument("--cut-only", action="store_true", help="record nothing, cut the clips")
    parser.add_argument(
        "--guest",
        default="",
        help="the ssh host of a handheld whose openSU guest records the remote-play shot",
    )
    return parser.parse_args(argv)


def _record(opensu: Path, log: Path, shots: tuple[Shot, ...], wanted: set[str], fmt) -> None:
    if not any(shot.name in wanted for shot in shots):
        return
    with HiddenShell(opensu, RealHome(), log) as shell:
        _library_loaded(shell)
        for shot in shots:
            shot.reach(shell)
            time.sleep(0.6)
            if shot.name in wanted:
                with Recorder(shot.sources(shell), WORK / "takes" / shot.name, fmt.fps) as taken:
                    shot.act(shell)
                clip = WORK / "clips" / f"{shot.name}.mp4"
                length = encode(taken.streams, clip, fmt.width, fmt.height, fmt.fps)
                repeated = ", ".join(
                    f"{stream.path.stem} {stream.repeated}/{stream.frames}"
                    for stream in taken.streams
                )
                print(f"{shot.name}: {length:.1f} s, frames repeated {repeated} -> {clip}")
            else:
                shot.act(shell)
            shot.leave(shell)


def main(argv: list[str] | None = None) -> int:
    args = _parse(sys.argv[1:] if argv is None else argv)
    fmt = edit.Format(args.width, args.height, args.fps)
    os.environ["OPENSU_WIDTH"] = str(fmt.width)
    os.environ["OPENSU_HEIGHT"] = str(fmt.height)
    os.environ.setdefault("OPENSU_DISPLAY_SCALE", f"{fmt.height / 720:g}")
    shots = {shot.name: shot for shot in (*home_shots(), *game_shots(), *theme_shots())}
    shots[remote.SHOT.name] = remote.SHOT
    wanted = set() if args.cut_only else (set(filter(None, args.only.split(","))) or set(shots))
    if wanted - set(shots):
        print(f"no shots named {sorted(wanted - set(shots))}; there are {CUT}", file=sys.stderr)
        return 1
    if remote.SHOT.name in wanted and not args.guest:
        if args.only:
            print("the remote-play shot needs --guest", file=sys.stderr)
            return 1
        wanted.discard(remote.SHOT.name)
    (WORK / "clips").mkdir(parents=True, exist_ok=True)
    if wanted:
        if not args.opensu.is_file():
            print(
                f"no openSU executable at {args.opensu}; build it or pass --opensu", file=sys.stderr
            )
            return 1
        log = WORK / "opensu.log"
        with real.KeptSettings(WORK / "settings-backup"):
            real.choose_console_theme(real.DEFAULT_CONSOLE_THEME)
            _record(args.opensu, log, home_shots() + game_shots(), wanted, fmt)
            real.choose_interface_theme(CHANNELS)
            _record(args.opensu, log, theme_shots(), wanted, fmt)
            real.choose_interface_theme("")
            if remote.SHOT.name in wanted:
                remote.record(args.opensu, args.guest, WORK / "remote", WORK / "clips", fmt)
    cut = WORK / "cut"
    shutil.rmtree(cut, ignore_errors=True)
    cut.mkdir(parents=True)
    parts = [edit.card(TITLE, 4.0, fmt, cut, cut / "00-title.mp4")]
    for index, name in enumerate(CUT, start=1):
        shot = shots[name]
        clip = WORK / "clips" / f"{name}.mp4"
        if not clip.is_file():
            if name == remote.SHOT.name:
                print("no remote-play clip; record it with --only remote --guest HOST")
                continue
            print(f"no clip of {name} yet; record it with --only {name}", file=sys.stderr)
            return 1
        parts.append(
            edit.captioned(
                clip, shot.caption, shot.longest, fmt, cut, cut / f"{index:02d}-{name}.mp4"
            )
        )
    parts.append(
        edit.list_card(
            "And more", FEATURES, 9.0, fmt, cut, cut / f"{len(CUT) + 1:02d}-features.mp4"
        )
    )
    parts.append(edit.card(END, 6.0, fmt, cut, cut / f"{len(CUT) + 2:02d}-end.mp4"))
    length = edit.total_length(parts)
    track = music.render(length, WORK / "music.wav")
    out = edit.assemble(parts, track, fmt, args.out)
    print(f"trailer: {out} ({length:.1f} s)")
    return 0
