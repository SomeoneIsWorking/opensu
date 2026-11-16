"""The trailer's shots: what each one records and the input played while it does.

A shot starts and ends on Home with nothing open, as the screenshot scenes do, but for the game's
shots, which run in order over one game. `reach` and `leave` are not recorded; `act` is, paced
for a viewer rather than as fast as the shell takes input.
"""

import time
import urllib.request
from collections.abc import Callable
from dataclasses import dataclass

from tools.screenshots import retro
from tools.screenshots.scenes import (
    DOWNLOAD_TIMEOUT,
    _close_guide,
    _close_search,
    _close_settings,
    _details,
    _downloaded,
    _guide,
    _guide_row,
    _into_console,
    _leave_console_shelf,
    _library,
    _nothing,
    _runner_past,
    _settings,
    _walk_to,
)
from tools.screenshots.shell import HiddenShell, Profile
from tools.trailer.recorder import Picture, Source

Action = Callable[[HiddenShell], None]
# A press a viewer can follow: the focus ring's move and a beat on the new tile.
STEP = 0.55
CUPHEAD = "steam:268910"


def _shell(shell: HiddenShell) -> Source:
    def grab() -> Picture:
        width, height, data = shell.frame_rgba()
        return Picture(data, width, height)

    return grab


def _game(shell: HiddenShell, home: Profile) -> Source:
    def grab() -> Picture:
        port = retro.runner_port(shell, home)
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/frame.rgba", timeout=30) as response:
            width = int(response.headers["X-Opensu-Width"])
            height = int(response.headers["X-Opensu-Height"])
            return Picture(response.read(), width, height)

    return grab


@dataclass(frozen=True)
class Shot:
    name: str
    caption: str
    act: Action
    reach: Action = _nothing
    leave: Action = _nothing
    # Longer takes are sped up to fit.
    longest: float = 7.0
    sources: Callable[[HiddenShell], dict[str, Source]] = lambda shell: {"shell": _shell(shell)}


def _hold(seconds: float) -> Action:
    return lambda _shell: time.sleep(seconds)


def _steps(*buttons: str, pause: float = STEP) -> Action:
    def act(shell: HiddenShell) -> None:
        for button in buttons:
            shell.press(button, settle=pause)

    return act


def _then(*actions: Action) -> Action:
    def act(shell: HiddenShell) -> None:
        for action in actions:
            action(shell)

    return act


def _focus_cuphead(shell: HiddenShell) -> None:
    """Cuphead, the README's details game, whose SteamGridDB cover fits. The walk only
    goes forward, and an earlier shot may have left the focus past it, so it starts from the
    first tile."""
    while True:
        before = shell.state().get("focusIndex")
        shell.press("left", settle=0.15)
        if shell.state().get("focusIndex") == before:
            break
    _walk_to(shell, lambda focused: focused == CUPHEAD, CUPHEAD, 60)


def _to_consoles(shell: HiddenShell) -> None:
    """Along Library's tiles to the first console's; `_to_console_tile` would press RB again."""
    _walk_to(shell, lambda focused: focused.startswith("console:"), "a console tile", 48)


def _leave_details(shell: HiddenShell) -> None:
    """Back from a game's details to Home, through the shelf it was opened from if any."""
    shell.press("b")
    shell.wait_for("detailsOpen", False)
    if shell.state().get("shelf") != "home":
        shell.press("b")
        shell.wait_for("shelf", "home")


def _search(shell: HiddenShell) -> None:
    shell.press("search", settle=0.8)
    shell.wait_for("searchOpen", True)
    for letter in "Super":
        shell.type_text(letter)
        time.sleep(0.22)
    shell.wait_for("searchText", "Super")
    time.sleep(2.5)


def _settings_tabs(shell: HiddenShell) -> None:
    _settings(shell)
    time.sleep(1.0)
    for _ in range(5):
        shell.press("r1", settle=0.9)


def home_shots() -> tuple[Shot, ...]:
    """The player's own library."""
    return (
        Shot(
            "home",
            "Every installed game, from every store, on one Home",
            _then(_hold(1.2), _steps(*["right"] * 9), _hold(0.8)),
            longest=7.0,
        ),
        Shot(
            "library",
            "Steam, Epic, GOG and every console in one Library",
            _then(
                _hold(0.4),
                _library,
                _hold(1.0),
                _to_consoles,
                _hold(0.8),
                _steps("right", "right", "right"),
                _steps("a", pause=1.2),
                _steps("right", "right", "right", "right"),
                _hold(0.8),
            ),
            leave=_leave_console_shelf,
            longest=9.0,
        ),
        Shot(
            "details",
            "Art, details and launch options for every game",
            _then(_details, _hold(2.2), _steps("down", "down", "up", "up"), _hold(0.6)),
            reach=_focus_cuphead,
            leave=_leave_details,
            longest=6.5,
        ),
        Shot(
            "search",
            "Find any game as you type",
            _search,
            leave=_close_search,
            longest=5.0,
        ),
        Shot(
            "guide",
            "The Guide button, from anywhere",
            _then(_guide, _hold(0.6), _steps("down", "down", "down", "down", "down"), _hold(0.6)),
            leave=_close_guide,
            longest=5.0,
        ),
        Shot(
            "settings",
            "Every setting, from the pad",
            _settings_tabs,
            leave=_close_settings,
            longest=6.0,
        ),
    )


def _runner(home: Profile) -> Callable[[HiddenShell], dict[str, Source]]:
    return lambda shell: {"game": _game(shell, home)}


def _runner_and_shell(home: Profile) -> Callable[[HiddenShell], dict[str, Source]]:
    return lambda shell: {"game": _game(shell, home), "ui": _shell(shell)}


def game_shots(home: Profile) -> tuple[Shot, ...]:
    """The homebrew game on its built-in core with the console's CRT shader, then the Guide menu
    and the layered settings over it."""

    def start_game(shell: HiddenShell) -> None:
        _into_console(shell, retro.SYSTEM)
        _details(shell)
        # A missing core is downloaded from the details page, as a player does.
        shell.press("down")
        if shell.state().get("screenFocus") == "details.download":
            shell.press("a")
            shell.wait_until(
                _downloaded(retro.CORE_DOWNLOAD), "the core downloaded", DOWNLOAD_TIMEOUT
            )
        shell.press("up")
        shell.wait_for("screenFocus", "details.launch")
        shell.press("a")
        shell.wait_for("inGame", True)
        shell.wait_until(lambda _state: _runner_past(shell, home), "the title screen", 60.0)

    def game_settings(shell: HiddenShell) -> None:
        _guide_row(shell, "down", "guide.gameSettings")
        shell.press("a", settle=1.0)
        shell.wait_for("screen", "layered")
        for _ in range(12):
            if shell.state().get("screenFocus") == "row/video.shader":
                break
            shell.press("down", settle=0.45)
        time.sleep(1.5)

    def close_game(shell: HiddenShell) -> None:
        shell.press("b")
        shell.wait_for("screen", "guide")
        _guide_row(shell, "up", "guide.closeGame")
        shell.press("a", "a")
        shell.wait_for("inGame", False)
        _leave_console_shelf(shell)

    return (
        Shot(
            "game",
            "ROMs on built-in libretro cores, with CRT shaders",
            _hold(5.0),
            reach=start_game,
            longest=5.0,
            sources=_runner(home),
        ),
        Shot(
            "guide-game",
            "Save states and settings without leaving the game",
            _then(_guide, _hold(0.8), _steps("down", "down", "down"), _hold(0.8)),
            longest=5.0,
            sources=_runner_and_shell(home),
        ),
        Shot(
            "layered",
            "Settings per console, core or game",
            game_settings,
            leave=close_game,
            longest=6.0,
            sources=_runner_and_shell(home),
        ),
    )
