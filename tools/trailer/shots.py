"""The trailer's shots: what each one records and the input played while it does.

Every shot runs on the player's own profile and games. A shot starts and ends on Home with nothing
open, as the screenshot scenes do, but for the games' shots, which run in order over one game
each. `reach` and `leave` are not recorded; `act` is, paced for a viewer rather than as fast as the
shell takes input.
"""

import time
from collections.abc import Callable
from dataclasses import dataclass

from tools.screenshots import retro
from tools.screenshots.scenes import (
    _close_guide,
    _close_search,
    _close_settings,
    _details,
    _game_mapping,
    _guide,
    _guide_row,
    _into_console,
    _leave_console_shelf,
    _library,
    _nothing,
    _settings,
    _walk_to,
)
from tools.screenshots.shell import HiddenShell, RealHome

Action = Callable[[HiddenShell], None]
# A press a viewer can follow: the focus ring's move and a beat on the new tile.
STEP = 0.55
CUPHEAD = "steam:268910"


# A source names the control channel that records it, looked up when the take starts.
Source = Callable[[], str]
Sources = Callable[[HiddenShell], dict[str, Source]]


def _shell(shell: HiddenShell) -> Source:
    return lambda: shell.url


def _game(shell: HiddenShell) -> Source:
    return lambda: f"http://127.0.0.1:{retro.runner_port(shell, RealHome())}"


def _only_shell(shell: HiddenShell) -> dict[str, Source]:
    return {"shell": _shell(shell)}


def _game_alone(shell: HiddenShell) -> dict[str, Source]:
    return {"game": _game(shell)}


def _game_and_shell(shell: HiddenShell) -> dict[str, Source]:
    return {"game": _game(shell), "ui": _shell(shell)}


@dataclass(frozen=True)
class Shot:
    name: str
    caption: str
    act: Action
    reach: Action = _nothing
    leave: Action = _nothing
    # Longer takes are sped up to fit.
    longest: float = 7.0
    sources: Sources = _only_shell


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


class _Game:
    """A ROM of the player's, started from its console's shelf on its built-in core."""

    def __init__(self, system: str, file: str, frames: int) -> None:
        self.system = system
        self.file = file
        # Frames run before the shot, past the core's boot to something worth showing.
        self.frames = frames

    def focus(self, shell: HiddenShell) -> None:
        """The console's shelf, the game focused."""
        _into_console(shell, self.system)
        _walk_to(shell, lambda focused: focused.endswith("/" + self.file), self.file, 400)

    def start(self, shell: HiddenShell) -> None:
        self.focus(shell)
        _details(shell)
        for _ in range(6):
            if shell.state().get("screenFocus") == "details.launch":
                break
            shell.press("up")
        shell.wait_for("screenFocus", "details.launch")
        shell.press("a")
        shell.wait_for("inGame", True, timeout=30)
        shell.wait_until(lambda _state: self._past(shell), f"{self.file} running", 120)

    def _past(self, shell: HiddenShell) -> bool:
        try:
            return retro.runner_frames(shell, RealHome()) >= self.frames
        except (OSError, RuntimeError):
            # The runner announces its port a moment after the shell says the game runs.
            return False

    @staticmethod
    def close(shell: HiddenShell) -> None:
        """The Guide's Close game, then out of the console's shelf to Home."""
        if not shell.state().get("guideMenuOpen"):
            _guide(shell)
        _guide_row(shell, "up", "guide.closeGame")
        shell.press("a", "a")
        shell.wait_for("inGame", False)
        _back_home(shell)


METAL_SLUG = _Game("arcade", "mslug3.zip", 900)
OCARINA = _Game("n64", "Legend of Zelda, The - Ocarina of Time (USA).z64", 1500)


def _layered(shell: HiddenShell) -> None:
    _guide_row(shell, "down", "guide.gameSettings")
    shell.press("a", settle=1.0)
    shell.wait_for("screen", "layered")
    time.sleep(0.8)
    for _ in range(5):
        shell.press("down", settle=0.6)
    time.sleep(1.0)


def _close_layered_game(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("screen", "guide")
    _Game.close(shell)


def _to_mapping(shell: HiddenShell) -> None:
    """Ocarina of Time's details, Game settings, Controls: its button board, A focused."""
    OCARINA.focus(shell)
    _game_mapping(shell)


def _mapping(shell: HiddenShell) -> None:
    """Along the board's buttons, then A's outputs up beside its chip."""
    time.sleep(1.0)
    for button in ("right", "right", "up", "left", "down"):
        shell.press(button, settle=0.6)
    shell.press("a", settle=1.6)
    shell.press("b", settle=0.6)


def _back_home(shell: HiddenShell) -> None:
    """Out of whatever is open (a mapping board, Game settings, details, a shelf) to Home."""
    for _ in range(8):
        state = shell.state()
        if (
            state.get("shelf") == "home"
            and not state.get("detailsOpen")
            and not state.get("screen")
        ):
            return
        if state.get("shelf") not in ("home", "library") and not state.get("detailsOpen"):
            _leave_console_shelf(shell)
            return
        shell.press("b", settle=0.5)
    raise RuntimeError(f"never back on Home: {shell.state()}")


def game_shots() -> tuple[Shot, ...]:
    """Metal Slug 3 and Ocarina of Time on their built-in cores, with the Guide, the layered
    settings and the button mapping over them."""
    return (
        Shot(
            "arcade",
            "Arcade and console games on built-in libretro cores",
            _hold(6.0),
            reach=METAL_SLUG.start,
            longest=6.0,
            sources=_game_alone,
        ),
        Shot(
            "guide-game",
            "Save states and settings without leaving the game",
            _then(_guide, _hold(0.8), _steps("down", "down", "down"), _hold(0.8)),
            leave=_Game.close,
            longest=5.0,
            sources=_game_and_shell,
        ),
        Shot(
            "n64",
            "From the arcade to the N64",
            _hold(6.0),
            reach=OCARINA.start,
            longest=6.0,
            sources=_game_alone,
        ),
        Shot(
            "layered",
            "Settings per console, core or game",
            _then(_guide, _layered),
            leave=_close_layered_game,
            longest=6.0,
            sources=_game_and_shell,
        ),
        Shot(
            "mapping",
            "Remap any button, for one game or all of them",
            _mapping,
            reach=_to_mapping,
            leave=_back_home,
            longest=6.0,
        ),
    )


def theme_shots() -> tuple[Shot, ...]:
    """Home again in the Channels interface theme."""
    return (
        Shot(
            "channels",
            "Interface themes: this is Channels",
            _then(_hold(1.0), _steps(*["right"] * 6), _hold(0.8)),
            longest=6.0,
        ),
    )
