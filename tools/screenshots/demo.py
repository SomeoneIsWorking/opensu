"""The demo library: freely licensed games, their art and where it comes from.

Store titles are fake Steam installs (an app manifest and an install folder) whose cover is the
project's own freely licensed art from Wikimedia Commons, never Steam's store art. ROM titles are
empty placeholder files, so their tiles keep openSU's own fallback.
"""

from dataclasses import dataclass
from pathlib import Path

from tools.download import fetch_verified, sha256_file

COMMONS = "https://upload.wikimedia.org/wikipedia/commons"


@dataclass(frozen=True)
class Art:
    url: str
    sha256: str
    page: str
    author: str
    licence: str

    @property
    def file_name(self) -> str:
        return self.url.rsplit("/", 1)[-1]


@dataclass(frozen=True)
class SteamGame:
    app_id: str
    name: str
    art: Art


@dataclass(frozen=True)
class RomGame:
    folder: str
    file_name: str


STEAM_GAMES: tuple[SteamGame, ...] = (
    SteamGame(
        "1572920",
        "SuperTux",
        Art(
            f"{COMMONS}/f/f1/Intro_%28SuperTux%29.jpg",
            "54da368b38977c10f57da0133b72f90bbe56d8d8826a1a7196494e9e9f9cf940",
            "https://commons.wikimedia.org/wiki/File:Intro_(SuperTux).jpg",
            "SuperTux Devel Team",
            "GPL",
        ),
    ),
    SteamGame(
        "599390",
        "Battle for Wesnoth",
        Art(
            f"{COMMONS}/6/67/The_Battle_for_Wesnoth_1.12_title_screen.png",
            "cf86d4b685ad2e2d15f6d0090d02a5edd4920e5bfef40cf858d9fe34123b298a",
            "https://commons.wikimedia.org/wiki/File:The_Battle_for_Wesnoth_1.12_title_screen.png",
            "The Battle for Wesnoth developers team",
            "GPL",
        ),
    ),
    SteamGame(
        "1536610",
        "OpenTTD",
        Art(
            f"{COMMONS}/b/ba/OpenTTD-15.3-en.png",
            "2afc2c85193991d7bef2c1c54d7b9b3579b62fd27e1041f1faa4e646c75293e6",
            "https://commons.wikimedia.org/wiki/File:OpenTTD-15.3-en.png",
            "Screenshot: theki; game: OpenTTD with the OpenGFX graphics set",
            "GPL-2.0",
        ),
    ),
    SteamGame(
        "380840",
        "Teeworlds",
        Art(
            f"{COMMONS}/4/43/Teeworlds_Screenshot_Jungle_0.6.1.png",
            "9da4dced58e70e26052ac961827a3bc6d36e4bb6252690efefc46ca60b97858d",
            "https://commons.wikimedia.org/wiki/File:Teeworlds_Screenshot_Jungle_0.6.1.png",
            "Heinrich5991",
            "CC BY-SA 3.0",
        ),
    ),
    SteamGame(
        "404410",
        "Endless Sky",
        Art(
            f"{COMMONS}/d/d0/Endless_Sky_0.9.12_title_screen.png",
            "714296ffa5e172f621be9297376b3280a670fb709d20ffeebbd628fae6a82724",
            "https://commons.wikimedia.org/wiki/File:Endless_Sky_0.9.12_title_screen.png",
            "Endless Sky developers",
            "GPL-3.0",
        ),
    ),
    SteamGame(
        "1127400",
        "Mindustry",
        Art(
            f"{COMMONS}/0/01/The_Serpulo_Planet_-_Mindustry.jpg",
            "4f05383c2b9b00e79413284f47c7ad5cd551cbfc9800be6da8d758e824292ac4",
            "https://commons.wikimedia.org/wiki/File:The_Serpulo_Planet_-_Mindustry.jpg",
            "Mindustry",
            "GPL",
        ),
    ),
    SteamGame(
        "1769170",
        "Shattered Pixel Dungeon",
        Art(
            f"{COMMONS}/c/c8/Shattered_Pixel_Dungeon_title_screen_v3.3.8.png",
            "3f7be6c30cae044f3366530cb9bf2f766c0dc2398bc4c82526207b1bbbc64286",
            "https://commons.wikimedia.org/wiki/File:Shattered_Pixel_Dungeon_title_screen_v3.3.8.png",
            "Evan Debenham",
            "GPL-3.0",
        ),
    ),
)

ROM_GAMES: tuple[RomGame, ...] = (
    RomGame("Game Boy", "Tobu Tobu Girl.gb"),
    RomGame("Game Boy Color", "uCity.gbc"),
    RomGame("NES", "Super Tilt Bro.nes"),
)


def _manifest(game: SteamGame, install_dir: str) -> str:
    return (
        '"AppState"\n{\n'
        f'\t"appid"\t\t"{game.app_id}"\n'
        f'\t"name"\t\t"{game.name}"\n'
        '\t"StateFlags"\t\t"4"\n'
        f'\t"installdir"\t\t"{install_dir}"\n'
        "}\n"
    )


def fetch_all(downloads: Path) -> None:
    for game in STEAM_GAMES:
        fetch_verified(game.art.url, game.art.sha256, downloads / game.art.file_name)


def build(root: Path, downloads: Path) -> tuple[Path, Path]:
    """Writes the demo Steam root and ROM root under root; returns both."""
    steam = root / "steam"
    (steam / "config").mkdir(parents=True, exist_ok=True)
    apps = steam / "steamapps"
    for game in STEAM_GAMES:
        install_dir = game.name.replace(" ", "")
        (apps / "common" / install_dir).mkdir(parents=True, exist_ok=True)
        (apps / f"appmanifest_{game.app_id}.acf").write_text(_manifest(game, install_dir))
        art = downloads / game.art.file_name
        if sha256_file(art) != game.art.sha256:
            raise RuntimeError(f"{art} is not the pinned file; fetch it again")
        cache = steam / "appcache" / "librarycache" / game.app_id
        cache.mkdir(parents=True, exist_ok=True)
        suffix = art.suffix.lower()
        for name in ("library_600x900", "library_hero"):
            (cache / f"{name}{suffix}").write_bytes(art.read_bytes())
    roms = root / "roms"
    for rom in ROM_GAMES:
        folder = roms / rom.folder
        folder.mkdir(parents=True, exist_ok=True)
        (folder / rom.file_name).touch()
    return steam, roms
