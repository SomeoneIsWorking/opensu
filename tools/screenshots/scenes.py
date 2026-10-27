"""The screens captured and the input that reaches each from a fresh start on Home.

Every scene starts and ends on Home with nothing open. `readme` marks the scenes the README shows;
parity runs capture all of them.
"""

from collections.abc import Callable
from dataclasses import dataclass

from tools.screenshots import legendary, mixer
from tools.screenshots.shell import HiddenShell

# Long enough for a panel's slide and the focus ring's entrance to finish.
SETTLE_SECONDS = 1.2
# The volume display holds for 1.4 s after a change (ui/volume_osd.hpp); its fade-in is 110 ms.
VOLUME_SETTLE_SECONDS = 0.4
# Bounds a walk through tiles or Settings categories, so a missing target fails instead of looping.
LONGEST_WALK = 12
SEARCH_TEXT = "Super"
# Guide is held this long: past the 500 ms hold delay (input/chord_set.hpp), the list's 110 ms
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
    # Guide menu rows: Home, Library, Devices, Downloads, Settings.
    shell.press("guide", "down", "down", "down", "down", "a")
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
    # The pad's Guide + A chord, held over the channel: the Guide menu on Volume.
    shell.press("guide+a")
    shell.wait_for("screenFocus", "guide.volume")


def _close_quick(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("guideMenuOpen", False)


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
    """Back out of the shelf open in Library, to Home."""
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


def _downloading(state: dict[str, object]) -> bool:
    downloads = state.get("downloads")
    return isinstance(downloads, list) and any(
        line.get("key") == f"epic:{legendary.APP_NAME}"
        and line.get("state") == "downloading"
        and line.get("percent") == legendary.PERCENT
        for line in downloads
    )


def _download_state(key: str, wanted: str) -> Callable[[dict[str, object]], bool]:
    def holds(state: dict[str, object]) -> bool:
        downloads = state.get("downloads")
        return isinstance(downloads, list) and any(
            line.get("key") == key and line.get("state") == wanted for line in downloads
        )

    return holds


def _epic_game(shell: HiddenShell, app_name: str) -> None:
    """Library, into the Epic launcher, then along it to the stand-in's `app_name`."""
    _library(shell)
    # The launchers fill Library's first column, Steam first.
    for _ in range(LONGEST_WALK):
        if shell.state().get("focusedId") == "launcher:epic":
            break
        shell.press("down")
    shell.wait_for("focusedId", "launcher:epic", timeout=1.0)
    shell.press("a")
    shell.wait_until(
        lambda state: str(state.get("focusedId", "")).startswith("epic:"), "an Epic game focused"
    )
    # Along the row, and down to the next one when the row ends.
    for _ in range(LONGEST_WALK):
        state = shell.state()
        if state.get("focusedId") == f"epic:{app_name}":
            return
        shell.press("right")
        if shell.state().get("focusIndex") == state.get("focusIndex"):
            shell.press("down")
    shell.wait_for("focusedId", f"epic:{app_name}", timeout=1.0)


def _update_badge(shell: HiddenShell) -> None:
    """The installed stand-in game focused in the Epic launcher, its tile showing the update."""
    _epic_game(shell, legendary.INSTALLED_APP_NAME)


def _details_update(shell: HiddenShell) -> None:
    """The installed stand-in game's details with Update focused."""
    _epic_game(shell, legendary.INSTALLED_APP_NAME)
    _details(shell)
    # Its buttons: Play, Update, Hide game, Uninstall, Controls, Back.
    shell.press("down")


def _arm_uninstall(shell: HiddenShell) -> None:
    """The installed stand-in game's details, and the first press on Uninstall."""
    _epic_game(shell, legendary.INSTALLED_APP_NAME)
    _details(shell)
    # Its buttons: Play, Update, Hide game, Uninstall, Controls, Back.
    shell.press("down", "down", "down", "a")
    shell.wait_for("armed", True)


def _leave_store_details(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("detailsOpen", False)
    _leave_console_shelf(shell)


def _start_epic_install(shell: HiddenShell) -> None:
    """Library, into the Epic launcher, the stand-in's game, Install; the panel hidden."""
    _epic_game(shell, legendary.APP_NAME)
    _details(shell)
    # Install on the details page, then Install on the panel it offers.
    shell.press("a", "a")
    shell.wait_until(_downloading, "the Epic install at its stand-in's percent")
    # B hides the install panel, B again closes the details page.
    shell.press("b", "b")
    shell.wait_for("detailsOpen", False)


def _open_downloads(shell: HiddenShell) -> None:
    # Guide menu rows: Home, Library, Devices, Downloads, Settings.
    shell.press("guide", "down", "down", "down", "a")
    shell.wait_for("downloadsOpen", True)


def _downloads(shell: HiddenShell) -> None:
    _start_epic_install(shell)
    _open_downloads(shell)


def _close_downloads_page(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("downloadsOpen", False)


def _close_downloads(shell: HiddenShell) -> None:
    """Downloads closed over the Epic launcher it was opened over, then back to Home."""
    _close_downloads_page(shell)
    _leave_console_shelf(shell)


def _downloads_uninstalling(shell: HiddenShell) -> None:
    """The install the downloads scene left waiting cancelled (one store job runs at a time), then
    the installed stand-in game uninstalled, then Downloads."""
    _open_downloads(shell)
    # The install's row is the only live one, focused with its one button, Cancel.
    shell.press("a")
    shell.wait_until(
        _download_state(f"epic:{legendary.APP_NAME}", "cancelled"), "the Epic install cancelled"
    )
    _close_downloads_page(shell)
    _arm_uninstall(shell)
    shell.press("a")
    shell.wait_until(
        _download_state(f"epic:{legendary.INSTALLED_APP_NAME}", "uninstalling"),
        "the stand-in game uninstalling",
    )
    shell.press("b")
    shell.wait_for("detailsOpen", False)
    _leave_console_shelf(shell)
    # Its "uninstalling" toast would be caught mid-fade.
    shell.wait_for("toast", "", timeout=10.0)
    _open_downloads(shell)


# The most buttons a details page has: Play, Update, Emulator, Hide game, Uninstall, Choose artwork,
# Controls, Back.
LONGEST_DETAILS = 8


def _game_mapping(shell: HiddenShell) -> None:
    """The focused game's details, then Controls, the button before Back; the board opens on A."""
    _details(shell)
    # Focus stops at the last button, so enough Downs reach Back.
    shell.press(*(["down"] * LONGEST_DETAILS), "up", "a")
    shell.wait_for("mapping", "board")
    shell.wait_for("mappingFocus", "button.a")


def _leave_game_mapping(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("mapping", "")
    _back_from_details(shell)


def _mapping_picker(shell: HiddenShell) -> None:
    """The game's mapping with A's outputs up beside its chip."""
    _game_mapping(shell)
    shell.press("a")
    shell.wait_for("mapping", "picking")


def _leave_mapping_picker(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("mapping", "board")
    _leave_game_mapping(shell)


def _mapping_listening(shell: HiddenShell) -> None:
    """The game's mapping listening for a chord: Add chord is left along the strip under A."""
    _game_mapping(shell)
    shell.press("down")
    shell.wait_for("mappingFocus", "steam")
    shell.press("left")
    shell.wait_for("mappingFocus", "add")
    shell.press("a")
    shell.wait_for("mapping", "listening")


def _leave_mapping_listening(shell: HiddenShell) -> None:
    # B alone stops listening.
    shell.press("b")
    shell.wait_for("mapping", "board")
    _leave_game_mapping(shell)


def _chord_picker(shell: HiddenShell) -> None:
    """L2 + A heard while listening, its outputs up: actions, buttons and Select + buttons."""
    _mapping_listening(shell)
    shell.press("l2+a")
    shell.wait_for("mapping", "picking")


def _steam_input_picker(shell: HiddenShell) -> None:
    """The game's Steam Input settings, from its chip on the strip under A."""
    _game_mapping(shell)
    shell.press("down")
    shell.wait_for("mappingFocus", "steam")
    shell.press("a")
    shell.wait_for("mapping", "picking")


def _mapping_changed(shell: HiddenShell) -> None:
    """A sending B and L2 + A opening the quick menu: the changed callout, a chord and Reset."""
    _game_mapping(shell)
    # B's tile is right of A's, where the picker opens.
    shell.press("a", "right", "a")
    shell.wait_for("mapping", "board")
    shell.press("down")
    shell.wait_for("mappingFocus", "steam")
    shell.press("left")
    shell.wait_for("mappingFocus", "add")
    shell.press("a")
    shell.wait_for("mapping", "listening")
    shell.press("l2+a")
    shell.wait_for("mapping", "picking")
    # Quick menu, the first action, is where a new chord's picker opens.
    shell.press("a")
    shell.wait_for("mapping", "board")
    shell.wait_for("mappingFocus", "add")


def _leave_mapping_changed(shell: HiddenShell) -> None:
    """Reset, the strip's last chip, gives the game back what it inherits."""
    shell.press("right")
    shell.wait_for("mappingFocus", "steam")
    shell.press("right")
    shell.wait_for("mappingFocus", "reset")
    shell.press("a")
    shell.wait_until(lambda state: state.get("mappingFocus") != "reset", "the reset done")
    _leave_game_mapping(shell)


def _global_mapping(shell: HiddenShell) -> None:
    """Settings › Controls, then its first row, the mapping for every game."""
    _settings_page("Controls")(shell)
    shell.press("a", "a")
    shell.wait_for("mapping", "board")


def _leave_global_mapping(shell: HiddenShell) -> None:
    shell.press("b")
    shell.wait_for("mapping", "")
    # B leaves the rows for the categories, then closes Settings.
    shell.press("b")
    _close_settings(shell)


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
    Scene("quick-menu", "Guide menu on Volume (Guide + A)", _quick, _close_quick),
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
    Scene("game-mapping", "A game's button mapping", _game_mapping, _leave_game_mapping),
    Scene(
        "mapping-picker",
        "A button's outputs in a game's mapping",
        _mapping_picker,
        _leave_mapping_picker,
    ),
    Scene(
        "mapping-listening",
        "A game's mapping listening for a chord",
        _mapping_listening,
        _leave_mapping_listening,
    ),
    Scene("chord-picker", "A new chord's outputs", _chord_picker, _leave_mapping_picker),
    Scene(
        "steam-input-picker",
        "A game's Steam Input settings",
        _steam_input_picker,
        _leave_mapping_picker,
    ),
    Scene(
        "mapping-changed",
        "A game's mapping with a changed button and a chord",
        _mapping_changed,
        _leave_mapping_changed,
    ),
    Scene(
        "global-mapping",
        "Settings: the button mapping for every game",
        _global_mapping,
        _leave_global_mapping,
    ),
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

# They need a run of their own with the stand-in `legendary` (legendary.py) on PATH: its Epic
# launcher would change every other scene.
# In this order: the uninstall waits for the install, which only the last scene cancels.
DOWNLOAD_SCENES: tuple[Scene, ...] = (
    Scene(
        "update-badge",
        "An installed Epic game with an update: the badge on its tile",
        _update_badge,
        _leave_console_shelf,
    ),
    Scene(
        "details-update",
        "Details: Update for an installed Epic game a build behind",
        _details_update,
        _leave_store_details,
    ),
    Scene(
        "details-uninstall",
        "Details: Uninstall waiting for its second press",
        _arm_uninstall,
        _leave_store_details,
    ),
    Scene("downloads", "Downloads: an Epic install in progress", _downloads, _close_downloads),
    Scene(
        "downloads-uninstalling",
        "Downloads: an Epic uninstall in progress and a cancelled install",
        _downloads_uninstalling,
        _close_downloads_page,
    ),
)
