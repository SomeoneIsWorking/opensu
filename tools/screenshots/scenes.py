"""The screens captured and the input that reaches each from a fresh start on Home.

Every scene starts and ends on Home with nothing open. `readme` marks the scenes the README shows;
parity runs capture all of them.
"""

from collections.abc import Callable
from dataclasses import dataclass

from tools.screenshots import mixer
from tools.screenshots.shell import HiddenShell

# Long enough for a panel's slide and the focus ring's entrance to finish.
SETTLE_SECONDS = 1.2
# The volume display holds for 1.4 s after a change (ui/volume_osd.hpp); its fade-in is 110 ms.
VOLUME_SETTLE_SECONDS = 0.4
# Bounds a walk through tiles or Settings categories, so a missing target fails instead of looping.
LONGEST_WALK = 12
SEARCH_TEXT = "Super"
# Guide is held this long: past the 500 ms hold delay (input/pad_chords.hpp), the list's 110 ms
# fade-in and the frame's capture.
CHORD_HOLD = "3000ms"
# The chord list's fade-in is 110 ms (ui/chord_hint.hpp); the frame must come well inside the hold.
CHORD_SETTLE_SECONDS = 0.4


@dataclass(frozen=True)
class Scene:
    name: str
    caption: str
    reach: Callable[[HiddenShell], None]
    leave: Callable[[HiddenShell], None]
    readme: bool = False
    settle: float = SETTLE_SECONDS


def _nothing(_shell: HiddenShell) -> None:
    pass


def _library(shell: HiddenShell) -> None:
    shell.press("r1")
    shell.wait_for("section", "library")


def _home(shell: HiddenShell) -> None:
    shell.press("l1")
    shell.wait_for("section", "home")


def _details(shell: HiddenShell) -> None:
    shell.press("a")
    shell.wait_for("detailsOpen", True)


def _back_from_details(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("detailsOpen", False)


def _guide(shell: HiddenShell) -> None:
    shell.press("guide")
    shell.wait_for("guideMenuOpen", True)


def _close_guide(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("guideMenuOpen", False)


def _settings(shell: HiddenShell) -> None:
    # Guide menu rows: Home, Library, Devices, Settings.
    shell.press("guide", "down", "down", "down", "a")
    shell.wait_for("settingsOpen", True)


def _close_settings(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("settingsOpen", False)


def _settings_page(label: str) -> Callable[[HiddenShell], None]:
    """Settings, then down the category list until the trail names `label`."""
    trail = f"Settings > {label}"

    def reach(shell: HiddenShell) -> None:
        _settings(shell)
        for _ in range(LONGEST_WALK):
            if shell.state().get("breadcrumb") == trail:
                return
            shell.press("down")
        shell.wait_for("breadcrumb", trail, timeout=1.0)

    return reach


def _quick(shell: HiddenShell) -> None:
    # The pad's Guide + A chord, held over the channel.
    shell.press("guide+a")
    shell.wait_for("quickMenuOpen", True)


def _close_quick(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("quickMenuOpen", False)


def _console_shelf(shell: HiddenShell) -> None:
    """Library, right along the tiles to the first console, then into it."""
    _library(shell)
    for _ in range(LONGEST_WALK):
        focused = str(shell.state().get("focusedId", ""))
        if focused.startswith("console:"):
            shell.press("a")
            shell.wait_for("shelf", focused.removeprefix("console:"))
            return
        shell.press("right")
    raise RuntimeError(f"no console tile to the right in Library: {shell.state()}")


def _leave_console_shelf(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("shelf", "library")
    _home(shell)


def _context_menu(shell: HiddenShell) -> None:
    shell.press("select")
    shell.wait_for("contextMenuOpen", True)


def _close_context_menu(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("contextMenuOpen", False)


def _options_panel(shell: HiddenShell) -> None:
    _library(shell)
    shell.press("start")
    shell.wait_for("modeChooserOpen", True)


def _close_options_panel(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("modeChooserOpen", False)
    _home(shell)


def _search(shell: HiddenShell) -> None:
    shell.press("search")
    shell.wait_for("searchOpen", True)
    shell.type_text(SEARCH_TEXT)
    shell.wait_for("searchText", SEARCH_TEXT)


def _close_search(shell: HiddenShell) -> None:
    # Select clears the text; B on an empty field closes the panel.
    shell.press("select")
    shell.wait_for("searchText", "")
    shell.press("b")
    shell.wait_for("searchOpen", False)


def _volume(shell: HiddenShell) -> None:
    shell.key("ctrl+up", settle=0.0)
    shell.wait_until(
        lambda state: (
            state.get("volumeShown") is True and state.get("volumePercent") == mixer.PERCENT + 5
        ),
        "the volume display at one step up",
    )


def _volume_gone(shell: HiddenShell) -> None:
    shell.wait_for("volumeShown", False)


def _chord_hint(shell: HiddenShell) -> None:
    shell.post("/input", f"guide {CHORD_HOLD}")
    shell.wait_for("chordHint", "guide")


def _chord_hint_gone(shell: HiddenShell) -> None:
    shell.wait_for("chordHint", "")
    shell.wait_for("guideMenuOpen", False)


SCENES: tuple[Scene, ...] = (
    Scene("home", "Home: installed games from every store", _nothing, _nothing, readme=True),
    Scene(
        "library",
        "Library: stores, all games and one tile per console",
        _library,
        _home,
        readme=True,
    ),
    Scene("details", "A game's details page", _details, _back_from_details, readme=True),
    Scene("guide", "Guide menu", _guide, _close_guide, readme=True),
    Scene("settings", "Settings", _settings, _close_settings, readme=True),
    Scene("quick-menu", "Quick menu", _quick, _close_quick, readme=True),
    Scene("console-shelf", "A console's shelf in Library", _console_shelf, _leave_console_shelf),
    Scene("settings-library", "Settings: Library", _settings_page("Library"), _close_settings),
    Scene("settings-audio", "Settings: Audio", _settings_page("Audio"), _close_settings),
    Scene("settings-controls", "Settings: Controls", _settings_page("Controls"), _close_settings),
    Scene(
        "settings-guide-chords",
        "Settings: Guide chords",
        _settings_page("Guide chords"),
        _close_settings,
    ),
    Scene("settings-about", "Settings: About", _settings_page("About"), _close_settings),
    Scene("context-menu", "A game tile's menu", _context_menu, _close_context_menu),
    Scene("options-panel", "Library's options panel", _options_panel, _close_options_panel),
    Scene("search", "Search with results", _search, _close_search),
    Scene("volume", "The volume display", _volume, _volume_gone, settle=VOLUME_SETTLE_SECONDS),
    Scene(
        "chord-hint",
        "Guide held: its chords",
        _chord_hint,
        _chord_hint_gone,
        settle=CHORD_SETTLE_SECONDS,
    ),
)

README_SCENES: tuple[Scene, ...] = tuple(scene for scene in SCENES if scene.readme)
