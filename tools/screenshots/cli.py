"""Command line: `uv run --frozen python -m tools.screenshots [--profile demo|real] ...`.

`--profile real` runs openSU --hidden on the player's own home with the network, as their session
has it, and writes the README's WebPs to docs/screenshots/. It refuses while another openSU runs and
puts the player's settings files back afterwards. `--game`, `--console` and `--search` pick what it
shows. `--console-theme THEME` shows another console art theme (`--iisu-file FILE` for iiSU's,
iiSU's release APK or starter pack) and writes only Library in it, `library-<theme>`; the default
run writes `library-drawings` beside the README set.

`--profile demo` (the default) builds the demo library, then runs openSU --hidden offline in a
throwaway home and writes one WebP per demo scene to scratch/screenshots/demo/. With --parity DIR,
writes a raw PNG of every scene to DIR instead, with a fixed system volume, as references for
`tools.screenshots.compare`; the Downloads scenes run again in a home of their own with a stand-in
`legendary`.
"""

import argparse
import shutil
import sys
import time
from collections.abc import Callable
from pathlib import Path

from tools.screenshots import demo, encode, legendary, mixer, offline, real
from tools.screenshots.scenes import (
    DOWNLOAD_SCENES,
    README_SCENES,
    SCENES,
    Picks,
    Scene,
    real_scenes,
    theme_scenes,
)
from tools.screenshots.shell import HiddenShell, Home, Profile, RealHome

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "scratch" / "screenshots"
README_OUTPUT = REPO / "docs" / "screenshots"
DEMO_OUTPUT = WORK / "demo"
# The player's library is loaded once this many seconds pass with no new installed game.
LIBRARY_QUIET_SECONDS = 3.0
LIBRARY_TIMEOUT_SECONDS = 60.0


def _parse(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="python -m tools.screenshots", description=__doc__)
    parser.add_argument(
        "--opensu",
        type=Path,
        default=REPO / "build" / "check" / "cmake" / "src" / "opensu",
        help="the openSU executable to capture",
    )
    parser.add_argument(
        "--profile",
        choices=("demo", "real"),
        default="demo",
        help="the throwaway demo home offline, or the player's own home",
    )
    parser.add_argument("--out", type=Path, help="where the WebPs go")
    parser.add_argument(
        "--parity", type=Path, metavar="DIR", help="demo: write every scene as PNG into DIR instead"
    )
    parser.add_argument(
        "--game", default="", help="real: the game id Home focuses and details shows"
    )
    parser.add_argument("--console", default="", help="real: the system whose shelf is shown")
    parser.add_argument("--search", default=Picks.search, help="real: the text searched for")
    parser.add_argument(
        "--console-theme",
        default="drawings",
        help="real: the console art theme the run shows (drawings, photos, iisu or user/<name>); "
        "any but drawings writes only Library in it, as library-<theme>",
    )
    parser.add_argument(
        "--iisu-file", type=Path, help="real: iiSU's release APK or starter pack, for iisu"
    )
    args = parser.parse_args(argv)
    if args.console_theme == "iisu" and (args.iisu_file is None or not args.iisu_file.is_file()):
        parser.error("--console-theme iisu needs --iisu-file, iiSU's release APK or starter pack")
    if args.out is None:
        args.out = README_OUTPUT if args.profile == "real" else DEMO_OUTPUT
    return args


def _capture_real(args: argparse.Namespace) -> int:
    magick = encode.require_magick()
    if not args.opensu.is_file():
        print(f"no openSU executable at {args.opensu}; build it or pass --opensu", file=sys.stderr)
        return 1
    run = WORK / "real"
    frames = run / "frames"
    frames.mkdir(parents=True, exist_ok=True)
    picks = Picks(game=args.game, console=args.console, search=args.search)
    drawings = args.console_theme == "drawings"
    scenes = (
        real_scenes(picks) + theme_scenes(picks, "drawings")
        if drawings
        else theme_scenes(picks, args.console_theme.replace("/", "-"))
    )
    with real.KeptSettings(run / "settings-backup"):
        real.choose_console_theme(args.console_theme, args.iisu_file)
        _run(
            args.opensu,
            RealHome(),
            run / "opensu.log",
            scenes,
            frames,
            magick,
            args.out,
            _library_loaded,
        )
    return 0


def _library_loaded(shell: HiddenShell) -> None:
    """Waits until the installed count has held still for a while."""
    deadline = time.monotonic() + LIBRARY_TIMEOUT_SECONDS
    seen = -1
    still_since = time.monotonic()
    while time.monotonic() < deadline:
        installed = shell.state().get("installed")
        if installed != seen:
            seen = installed if isinstance(installed, int) else -1
            still_since = time.monotonic()
        elif seen > 0 and time.monotonic() - still_since >= LIBRARY_QUIET_SECONDS:
            return
        time.sleep(0.2)
    raise RuntimeError(f"the library never settled: {shell.state()}")


def _demo_loaded(store_installed: int) -> Callable[[HiddenShell], None]:
    """Waits for the demo library, and `store_installed` stand-in store games beside it."""

    def wait(shell: HiddenShell) -> None:
        installed = len(demo.STEAM_GAMES) + len(demo.ROM_GAMES) + store_installed
        shell.wait_for("installed", installed)

    return wait


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
    frames = parity if parity else run / "frames"
    frames.mkdir(parents=True, exist_ok=True)
    if not parity:
        profile = Profile(home=run / "home", steam_root=steam, rom_root=roms)
        _run(
            args.opensu,
            profile,
            run / "opensu.log",
            README_SCENES,
            frames,
            magick,
            args.out,
            _demo_loaded(0),
        )
        return 0
    tools = mixer.install(run / "tools")
    profile = Profile(home=run / "home", steam_root=steam, rom_root=roms, tools=tools)
    _run(args.opensu, profile, run / "opensu.log", SCENES, frames, None, args.out, _demo_loaded(0))
    store_tools = legendary.install(mixer.install(run / "store-tools"))
    store = Profile(home=run / "store-home", steam_root=steam, rom_root=roms, tools=store_tools)
    _run(
        args.opensu,
        store,
        run / "opensu-store.log",
        DOWNLOAD_SCENES,
        frames,
        None,
        args.out,
        _demo_loaded(legendary.INSTALLED_COUNT),
    )
    return 0


def _run(
    opensu: Path,
    profile: Home,
    log: Path,
    scenes: tuple[Scene, ...],
    frames: Path,
    magick: str | None,
    out: Path,
    loaded: Callable[[HiddenShell], None],
) -> None:
    """One hidden openSU in `profile`, capturing each of `scenes` into `frames` once `loaded`
    has waited for its library."""
    with HiddenShell(opensu, profile, log) as shell:
        loaded(shell)
        for scene in scenes:
            scene.reach(shell)
            time.sleep(scene.settle)
            shell.wait_for("artworkPending", 0)
            png = frames / f"{scene.name}.png"
            png.write_bytes(shell.frame())
            if magick is None:
                print(f"{scene.name}: {png}")
            else:
                webp = out / f"{scene.name}.webp"
                encode.to_webp(magick, png, webp)
                print(f"{scene.name}: {webp}")
            scene.leave(shell)


def _absolute(args: argparse.Namespace) -> list[str]:
    """The arguments again with every path absolute, as the offline run starts in the repo."""
    argv = ["--opensu", str(args.opensu.resolve()), "--out", str(args.out.resolve())]
    argv += ["--profile", args.profile]
    if args.parity:
        argv += ["--parity", str(args.parity.resolve())]
    return argv


def main(argv: list[str] | None = None) -> int:
    args = _parse(sys.argv[1:] if argv is None else argv)
    if args.profile == "real":
        return _capture_real(args)
    if not offline.is_offline():
        # Art is fetched first, while the network is still there.
        demo.fetch_all(WORK / "downloads")
        return offline.reexec_offline(_absolute(args))
    return _capture(args)
