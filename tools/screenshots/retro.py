"""The libretro runner's home: a freely licensed homebrew game played on a built-in core with a
shader preset, so the README can show a game without a commercial ROM.

The home is kept between runs, so the core and the shader pack openSU downloads into it come down
once. Saves go to that home, never the player's. A menu over the game is the runner's frame with
the shell's overlay frame over it, as Gamescope composes them for the player.
"""

import json
import subprocess
from dataclasses import dataclass
from pathlib import Path

from tools.download import fetch_verified
from tools.screenshots.shell import HiddenShell, Profile


@dataclass(frozen=True)
class Homebrew:
    url: str
    sha256: str
    page: str
    author: str
    licence: str
    folder: str
    file_name: str


UCITY = Homebrew(
    "https://github.com/AntonioND/ucity/releases/download/v1.3/ucity.gbc",
    "9422ee2ca7b7ea1d46b58b2a429fff3f354dfd3e732dee1e7ae6220f148ce6e0",
    "https://github.com/AntonioND/ucity",
    "Antonio Niño Díaz",
    "GPL-3.0-or-later; graphics and music CC BY-SA 4.0",
    "Game Boy Color",
    "uCity.gbc",
)
SYSTEM = "gbc"
# The Downloads page's key for the core openSU picks for it.
CORE_DOWNLOAD = "emulator:mgba-core"
# Set at the console's layer, so the game's own tab shows it inherited.
SHADER = "crt/crt-royale.slangp"
# Only the ROM library and the built-in runner, not the shell profile's every integration.
INTEGRATIONS = {
    "integrations": {
        "steam": False,
        "epic": False,
        "gog": False,
        "emulators": False,
        "plugins": False,
    }
}


def fetch(downloads: Path) -> Path:
    return fetch_verified(UCITY.url, UCITY.sha256, downloads / "homebrew" / UCITY.file_name)


def profile(root: Path, downloads: Path) -> Profile:
    """The kept home under `root`, its ROM root holding the homebrew game and its settings the
    console's shader preset."""
    roms = root / "roms"
    folder = roms / UCITY.folder
    folder.mkdir(parents=True, exist_ok=True)
    rom = folder / UCITY.file_name
    rom.write_bytes(fetch(downloads).read_bytes())
    steam = root / "steam"
    steam.mkdir(parents=True, exist_ok=True)
    home = root / "home"
    config = home / ".config" / "opensu"
    config.mkdir(parents=True, exist_ok=True)
    (config / "settings.json").write_text(json.dumps(INTEGRATIONS))
    layers = {"systems": {SYSTEM: {"video.shader": SHADER}}}
    (config / "game-settings.json").write_text(json.dumps(layers, indent=4))
    return Profile(home=home, steam_root=steam, rom_root=roms, user_session=True)


def shader_pack_installed(home: Profile) -> bool:
    """Whether openSU has kept the shader pack's record (`library/shader_pack`) in the home."""
    return (home.home / ".local" / "share" / "opensu" / "shaders" / "slang.json").is_file()


def runner_port(shell: HiddenShell, home: Profile) -> int:
    """The port the running game's `opensu-retro` announced in the control file `shell` gave it."""
    runtime = home.runtime() / "opensu"
    for control in sorted(runtime.glob(f"retro-{shell.pid}-*.json")):
        if control.name.endswith(".settings.json"):
            continue
        announced = json.loads(control.read_text())
        if "port" in announced:
            return int(announced["port"])
    raise RuntimeError(f"no runner control file in {runtime}")


def runner_frames(shell: HiddenShell, home: Profile) -> int:
    """How many frames the running game has run."""
    status = json.loads(shell.fetch(f"http://127.0.0.1:{runner_port(shell, home)}/status"))
    return int(status["frame"])


def runner_frame(shell: HiddenShell, home: Profile) -> bytes:
    return shell.fetch(f"http://127.0.0.1:{runner_port(shell, home)}/frame.png")


def over_game(shell: HiddenShell, home: Profile, work: Path) -> bytes:
    """The shell's frame composed over the runner's."""
    work.mkdir(parents=True, exist_ok=True)
    game = work / "game.png"
    overlay = work / "overlay.png"
    out = work / "composed.png"
    game.write_bytes(runner_frame(shell, home))
    overlay.write_bytes(shell.frame())
    subprocess.run(["magick", str(game), str(overlay), "-composite", str(out)], check=True)
    return out.read_bytes()
