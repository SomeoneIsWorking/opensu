"""The screens the README shows and the input that reaches each from a fresh start on Home."""

from collections.abc import Callable
from dataclasses import dataclass

from tools.screenshots.shell import HiddenShell


@dataclass(frozen=True)
class Scene:
    name: str
    caption: str
    reach: Callable[[HiddenShell], None]
    leave: Callable[[HiddenShell], None]


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


def _quick(shell: HiddenShell) -> None:
    # The pad's Guide + A chord cannot be held over the channel; the keyboard shortcut opens the
    # same menu, and a pad press after it brings back the pad's prompts.
    shell.key("ctrl+tab")
    shell.press("down", "up")
    shell.wait_for("quickMenuOpen", True)


SCENES: tuple[Scene, ...] = (
    Scene("home", "Home: installed games from every store", _nothing, _nothing),
    Scene("library", "Library: stores, all games and one tile per console", _library, _home),
    Scene("details", "A game's details page", _details, _back_from_details),
    Scene("guide", "Guide menu", _guide, _close_guide),
    Scene("settings", "Settings", _settings, _close_settings),
    Scene("quick-menu", "Quick menu", _quick, _nothing),
)
